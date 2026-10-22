#include "modules/device/DeviceBus.hpp"
#include "components/ComponentBuilder.tpp"
#include "DeviceLogic.hpp"
#include "components/GateListPartBehavioral.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryFFController.hpp"
#include "components/selection/FamilyRegistration.hpp"
#include "components/selection/FamilyFactories.hpp"
#include <bit>
using namespace circuit::device::detail;
namespace {
void portPins(IOComponent*p,size_t width=32) {
    circuit::memory::protocolPins(p,width);
    p->addPin("BACK_READY",PinType::INPUT);p->addPin<3>("BACK_STATUS",PinType::INPUT);p->addPinDynamic("BACK_READ_DATA",PinType::INPUT,width);
    for(auto n:{"READ_STROBE","WRITE_STROBE","IS_READ","IS_WRITE"})p->addPin(n,PinType::OUTPUT);
    p->addPin<32>("ADDRESS",PinType::OUTPUT);p->addPinDynamic("WRITE_DATA",PinType::OUTPUT,width);p->addPinDynamic("WRITE_MASK",PinType::OUTPUT,width/8);
}
using Region=circuit::device::AddressRegion;
const std::vector<Region> regions{{"RAM_",0,0x40000},{"CONSOLE_",0x10000000,0x100},
    {"TIMER_",0x10001000,0x100},{"BUTTONS_",0x10002000,0x100},{"DISPLAY_",0x10003000,0x100},{"VIDEO_",0x20000000,0x1000}};
void routerPins(IOComponent*p,const std::vector<Region>&r,size_t width=32) {
    circuit::memory::protocolPins(p,width);
    for(const auto&region:r)circuit::memory::protocolPins(p,width,region.prefix,true);
}
void buildRouter(IOComponent&root,ComponentBuilder&b,const std::vector<Region>&r,bool fetch,size_t width=32) {
    Logic l(b);auto in=[&](const std::string&s){return root.getInputPinDynamic(s);};
    auto running=l.inv(in("RST"));auto busy=l.reg("BUSY",1,in("CLK"),in("RST"),l.bit(true));
    auto sent=l.reg("SENT",1,in("CLK"),in("RST"),l.bit(true));
    auto active=busy->getOutputPin("Q"),issued=sent->getOutputPin("Q");
    auto ready=l.land({running,l.inv(active)}),accept=l.land({ready,in("REQ_VALID")});
    auto known=b.addNewComponent<circuit::memory_detail::RequestKnownBits>("KNOWN_FIELDS",width);
    for(auto pair:{std::pair{"REQ_OP","OP"},{"REQ_ADDR","ADDR"},{"REQ_WRITE_MASK","MASK"}})l.wire(in(pair.first),known->getInputPinDynamic(pair.second));
    auto op=l.split(in("REQ_OP"));auto address=l.split(in("REQ_ADDR"));
    auto read=l.equal({op,l.constant(3)}),write=l.equal({op,l.constant(3,1)});
    auto supported=fetch?read:l.lor({read,write});
    std::vector<P> selected;
    for(const auto&region:r) {
        auto low=std::countr_zero(region.bytes);auto base=l.constant(32,region.base);
        selected.push_back(l.equal({V(address.begin()+low,address.end()),V(base.begin()+low,base.end())}));
    }
    auto status=l.mux({l.any(selected),l.constant(3,2),l.constant(3)});
    status=l.mux({l.any(V(address.begin(),address.begin()+std::countr_zero(width/8))),status,l.constant(3,1)});
    status=l.mux({l.land({write,l.inv(known->getOutputPin("MASK_KNOWN"))}),status,l.constant(3,6)});
    status=l.mux({l.inv(supported),status,l.constant(3,4)});
    status=l.mux({l.inv(known->getOutputPin("FIELDS_KNOWN")),status,l.constant(3,6)});
    auto error=l.any(status);
    auto st=l.reg("STATUS",3,in("CLK"),in("RST"),accept);l.drive(status,st->getInputPin<3>("D"));
    std::map<std::string,std::shared_ptr<IOComponent>> held;
    for(auto pair:{std::pair{"OP",3u},{"ADDR",32u},{"WRITE_DATA",unsigned(width)},{"WRITE_MASK",unsigned(width/8)}}) {
        auto reg=l.reg("HELD_"+std::string(pair.first),pair.second,in("CLK"),in("RST"),accept);
        l.wire(in("REQ_"+std::string(pair.first)),reg->getInputPinDynamic("D"));held[pair.first]=reg;
    }
    auto held_error=l.any(l.split(st->getOutputPin<3>("Q")));
    auto hv=l.split(held["ADDR"]->getOutputPin<32>("Q"));
    V responses,requests;V data=l.constant(width),response_status=l.split(st->getOutputPin<3>("Q"));
    for(size_t i=0;i<r.size();++i) {
        auto owner=l.reg("OWNER_"+std::to_string(i),1,in("CLK"),in("RST"),accept);
        l.wire(l.land({selected[i],l.inv(error)}),owner->getInputPin("D"));
        auto own=owner->getOutputPin("Q");const auto&prefix=r[i].prefix;
        auto request=l.land({running,l.land({active,l.land({own,l.inv(issued)})})});
        auto grant=l.land({request,in(prefix+"REQ_READY")});requests.push_back(grant);
        auto pending=l.land({active,l.land({own,l.lor({issued,grant})})});
        auto response=l.land({pending,in(prefix+"RSP_VALID")});responses.push_back(response);
        l.out(request,root.getOutputPin(prefix+"REQ_VALID"));
        l.out(l.land({running,l.land({pending,in("RSP_READY")})}),root.getOutputPin(prefix+"RSP_READY"));
        l.out(in("CLK"),root.getOutputPin(prefix+"CLK"));l.out(in("RST"),root.getOutputPin(prefix+"RST"));
        for(auto pair:{std::pair{"OP",3u},{"WRITE_DATA",unsigned(width)},{"WRITE_MASK",unsigned(width/8)}})
            l.out(held[pair.first]->getOutputPinDynamic("Q"),root.getOutputPinDynamic(prefix+"REQ_"+pair.first));
        auto offset=l.constant(32);for(size_t j=0;j<std::countr_zero(r[i].bytes);++j)offset[j]=hv[j];
        l.drive(offset,root.getOutputPin<32>(prefix+"REQ_ADDR"));
        data=l.mux({own,data,l.split(in(prefix+"RSP_READ_DATA"))});
        response_status=l.mux({own,response_status,l.split(in(prefix+"RSP_STATUS"))});
    }
    auto valid=l.land({running,l.land({active,l.lor({held_error,l.any(responses)})})});
    auto done=l.land({valid,in("RSP_READY")});
    l.wire(l.lor({accept,l.land({active,l.inv(done)})}),busy->getInputPin("D"));
    l.wire(l.land({active,l.land({l.inv(done),l.lor({issued,l.any(requests)})})}),sent->getInputPin("D"));
    l.out(ready,root.getOutputPin("REQ_READY"));l.out(valid,root.getOutputPin("RSP_VALID"));
    l.drive(data,root.getOutputPinDynamic("RSP_READ_DATA"));l.drive(response_status,root.getOutputPin<3>("RSP_STATUS"));l.finish();
}
}
namespace circuit::device::detail {
P knownWord(Logic&l,ComponentBuilder&b,P data) {
    auto p=b.addNewComponent<circuit::memory_detail::RequestKnownBits>(l.id("DATA_KNOWN"),32);
    l.drive(l.constant(3),p->getInputPin<3>("OP"));l.drive(l.constant(4,15),p->getInputPin<4>("MASK"));
    l.wire(data,p->getInputPin<32>("ADDR"));return p->getOutputPin("FIELDS_KNOWN");
}
}
namespace {
void buildPort(IOComponent& root,ComponentBuilder& b,size_t width) {
    Logic l(b);auto clk=root.getInputPin("CLK"),rst=root.getInputPin("RST");auto running=l.inv(rst);
    auto pending=l.reg("PENDING",1,clk,rst,l.bit(true)),executed=l.reg("EXECUTED",1,clk,rst,l.bit(true));
    auto response=b.addByContract({"memory.shared.response-buffer."+std::to_string(width),"RESPONSE",{},{},{},{},circuit::Fidelity::Structural});
    for(auto pair:{std::pair{"CLK","CLK"},{"RST","RST"},{"RSP_READY","OUT_READY"}})l.wire(root.getInputPin(pair.first),response->getInputPin(pair.second));
    auto ready=l.land({running,l.land({l.inv(pending->getOutputPin("Q")),l.inv(response->getOutputPin("FULL"))})});
    auto accept=l.land({ready,root.getInputPin("REQ_VALID")});
    auto known=b.addNewComponent<circuit::memory_detail::RequestKnownBits>("KNOWN_FIELDS",width);
    for(auto pair:{std::pair{"REQ_OP","OP"},{"REQ_ADDR","ADDR"},{"REQ_WRITE_MASK","MASK"}})l.wire(root.getInputPinDynamic(pair.first),known->getInputPinDynamic(pair.second));
    auto op=l.split(root.getInputPin<3>("REQ_OP")),mask=l.split(root.getInputPinDynamic("REQ_WRITE_MASK")),address=l.split(root.getInputPin<32>("REQ_ADDR"));
    auto read=l.equal({op,l.constant(3)}),write=l.equal({op,l.constant(3,1)});
    auto status=l.constant(3);
    status=l.mux({l.any(V(address.begin(),address.begin()+std::countr_zero(width/8))),status,l.constant(3,1)});
    auto badmask=l.land({write,l.lor({l.inv(known->getOutputPin("MASK_KNOWN")),l.land({l.any(mask),l.inv(l.all(mask))})})});
    status=l.mux({badmask,status,l.constant(3,6)});status=l.mux({l.inv(l.lor({read,write})),status,l.constant(3,4)});
    status=l.mux({l.inv(known->getOutputPin("FIELDS_KNOWN")),status,l.constant(3,6)});
    auto checked=l.reg("CHECKED_STATUS",3,clk,rst,accept);l.drive(status,checked->getInputPin<3>("D"));
    auto heldop=l.reg("OP",3,clk,rst,accept);l.wire(root.getInputPin<3>("REQ_OP"),heldop->getInputPin<3>("D"));
    auto heldaddr=l.reg("ADDRESS",32,clk,rst,accept);l.wire(root.getInputPin<32>("REQ_ADDR"),heldaddr->getInputPin<32>("D"));
    auto helddata=l.reg("DATA",width,clk,rst,accept);l.wire(root.getInputPinDynamic("REQ_WRITE_DATA"),helddata->getInputPinDynamic("D"));
    auto heldmask=l.reg("MASK",width/8,clk,rst,accept);l.wire(root.getInputPinDynamic("REQ_WRITE_MASK"),heldmask->getInputPinDynamic("D"));
    op=l.split(heldop->getOutputPin<3>("Q"));mask=l.split(heldmask->getOutputPinDynamic("Q"));
    read=l.equal({op,l.constant(3)});write=l.equal({op,l.constant(3,1)});
    auto checkedstatus=l.split(checked->getOutputPin<3>("Q"));
    status=l.mux({l.any(checkedstatus),l.split(root.getInputPin<3>("BACK_STATUS")),checkedstatus});
    auto ok=l.inv(l.any(status)),action=l.land({write,l.land({l.any(mask),ok})});
    auto fire=l.land({running,l.land({pending->getOutputPin("Q"),l.land({l.inv(executed->getOutputPin("Q")),l.lor({l.inv(action),root.getInputPin("BACK_READY")})})})});
    l.wire(fire,response->getInputPin("IN_VALID"));l.drive(status,response->getInputPin<3>("IN_STATUS"));
    l.drive(l.mux({l.land({read,ok}),l.constant(width),l.split(root.getInputPinDynamic("BACK_READ_DATA"))}),response->getInputPinDynamic("IN_DATA"));
    auto take=l.land({response->getOutputPin("OUT_VALID"),root.getInputPin("RSP_READY")});
    l.wire(l.lor({accept,l.land({pending->getOutputPin("Q"),l.inv(take)})}),pending->getInputPin("D"));
    l.wire(l.land({l.inv(accept),l.land({l.inv(take),l.lor({fire,executed->getOutputPin("Q")})})}),executed->getInputPin("D"));
    l.out(ready,root.getOutputPin("REQ_READY"));
    for(auto pair:{std::pair{"OUT_VALID","RSP_VALID"},{"OUT_DATA","RSP_READ_DATA"},{"OUT_STATUS","RSP_STATUS"}})l.out(response->getOutputPinDynamic(pair.first),root.getOutputPinDynamic(pair.second));
    l.out(l.land({fire,l.land({read,ok})}),root.getOutputPin("READ_STROBE"));l.out(l.land({fire,action}),root.getOutputPin("WRITE_STROBE"));
    l.out(read,root.getOutputPin("IS_READ"));l.out(write,root.getOutputPin("IS_WRITE"));
    l.out(heldaddr->getOutputPin<32>("Q"),root.getOutputPin<32>("ADDRESS"));l.out(helddata->getOutputPinDynamic("Q"),root.getOutputPinDynamic("WRITE_DATA"));
    l.out(heldmask->getOutputPinDynamic("Q"),root.getOutputPinDynamic("WRITE_MASK"));l.finish();
}
}
namespace circuit::device {
namespace {
std::vector<Region> configuredRegions(const AddressMap& map){return {map.regions.begin(),map.regions.end()};}
void wordBusPins(IOComponent* p,BusUnit u,size_t w){
    if(w!=8 && w!=16 && w!=32)throw std::invalid_argument("Bus data width must be 8, 16 or 32");
    if(u==BusUnit::RegisterPort)portPins(p,w);
    else if(u==BusUnit::Router)routerPins(p,regions,w);
    else if(u==BusUnit::InstructionGuard)routerPins(p,{{"MEM_",0,0x40000}},w);
    else memory::protocolPins(p,w);
}
std::string busName(BusUnit u){switch(u){case BusUnit::RegisterPort:return "DeviceRegisterPort";case BusUnit::ErrorTarget:return "BusErrorTarget";
    case BusUnit::Router:return "AddressRouter";case BusUnit::InstructionGuard:return "InstructionAccessGuard";}throw std::invalid_argument("Invalid bus unit");}
}
std::string busId(BusUnit u,size_t w){const char* name=u==BusUnit::RegisterPort?"device.word.register-port.":u==BusUnit::Router?"bus.word.address-router.":u==BusUnit::ErrorTarget?"bus.word.error-target.":"bus.word.instruction-guard.";return name+std::to_string(w);}

WordBusComponent::WordBusComponent(std::string n,BusUnit u,size_t width,AddressMap map)
    :IOComponent(std::move(n),[u,width](IOComponent* p){wordBusPins(p,u,width);}),unit_(u),width_(width),map_(std::move(map)),type_(busName(u)+"<"+std::to_string(width)+">"){map_.validate();}
void WordBusComponent::buildInternals(ComponentBuilder& b){
    if(unit_==BusUnit::RegisterPort){buildPort(*this,b,width_);return;}
    if(unit_==BusUnit::Router){buildRouter(*this,b,configuredRegions(map_),false,width_);return;}
    if(unit_==BusUnit::InstructionGuard){buildRouter(*this,b,{{"MEM_",map_.regions[0].base,map_.regions[0].bytes}},true,width_);return;}
    detail::Logic l(b);auto p=b.add(busFamily(BusUnit::RegisterPort,width_),"PORT");
    for(auto n:{"CLK","RST","REQ_VALID","REQ_OP","REQ_ADDR","REQ_WRITE_DATA","REQ_WRITE_MASK","RSP_READY"})l.wire(getInputPinDynamic(n),p->getInputPinDynamic(n));
    for(auto n:{"REQ_READY","RSP_VALID","RSP_READ_DATA","RSP_STATUS"})l.out(p->getOutputPinDynamic(n),getOutputPinDynamic(n));
    l.wire(l.bit(true),p->getInputPin("BACK_READY"));l.drive(l.constant(3,2),p->getInputPin<3>("BACK_STATUS"));l.drive(l.constant(width_),p->getInputPinDynamic("BACK_READ_DATA"));l.finish();
}
std::string WordBusComponent::componentLayoutVariant()const {return detail::mapLayoutVariant(map_);}

const ComponentFamily& busFamily(BusUnit u,size_t width){
    if(width!=8 && width!=16 && width!=32)throw std::invalid_argument("Invalid bus width");
    struct Entry{std::string id,type;ComponentFamily f;
        Entry(BusUnit u,size_t w):id(busId(u,w)),type(busName(u)+"<"+std::to_string(w)+">"),
            f(id,type,[u,w](IOComponent*p){wordBusPins(p,u,w);},[u,w](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<WordBusComponent>(c,n,u,w,u==BusUnit::Router&&c?detail::addressMap(c->parameters()):AddressMap{});},
                w==32&&u!=BusUnit::ErrorTarget?FamilyFactory([u,w](const std::string& n,const std::shared_ptr<BuildContext>& c){return circuit::detail::createGateListPart(busFamily(u,w),n,c);}):FamilyFactory{}){}};
    static std::map<std::pair<BusUnit,size_t>,std::unique_ptr<Entry>> entries;auto& e=entries[{u,width}];if(!e)e=std::make_unique<Entry>(u,width);return e->f;
}
void registerWordBusComponents(ComponentCatalog& catalog) {
    for(size_t width:{8u,16u,32u})for(auto unit:{BusUnit::RegisterPort,BusUnit::ErrorTarget,BusUnit::Router,BusUnit::InstructionGuard}) {
        const auto& f=busFamily(unit,width);
        const std::string owner=width==32?busName(unit)+"Width32Test":"WidthComponentsTest";
        const ParameterPredicate supports=[unit](const ParameterMap& p){if(unit!=BusUnit::Router)return p.empty();try{(void)detail::addressMap(p);return true;}catch(const std::exception&){return false;}};
        registration::registerStructural(catalog,f,owner,"clock-boundary",supports);
        if(f.supports(Fidelity::Behavioral))registration::registerBehavioral(catalog,f,owner,busName(unit)+"Width32ScenarioTest",supports);
    }
}
}
