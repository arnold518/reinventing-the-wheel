#include "components/ComponentBuilder.tpp"
#include "DeviceLogic.hpp"
#include "components/GateListPartBehavioral.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/MemoryFFController.hpp"
#include "modules/rv32im/RegisterComponent.hpp"
#include "components/selection/FamilyFactories.hpp"
#include <bit>
using namespace circuit::device::detail;
namespace {
void portPins(IOComponent*p) {
    circuit::memory::protocolPins(p,32);
    p->addPin("BACK_READY",PinType::INPUT);p->addPin<3>("BACK_STATUS",PinType::INPUT);p->addPin<32>("BACK_READ_DATA",PinType::INPUT);
    for(auto n:{"READ_STROBE","WRITE_STROBE","IS_READ","IS_WRITE"})p->addPin(n,PinType::OUTPUT);
    for(auto n:{"ADDRESS","WRITE_DATA"})p->addPin<32>(n,PinType::OUTPUT);p->addPin<4>("WRITE_MASK",PinType::OUTPUT);
}
struct Region {std::string prefix;uint32_t base;uint64_t bytes;};
const std::vector<Region> regions{{"RAM_",0,0x40000},{"CONSOLE_",0x10000000,0x100},
    {"TIMER_",0x10001000,0x100},{"BUTTONS_",0x10002000,0x100},{"DISPLAY_",0x10003000,0x100},{"VIDEO_",0x20000000,0x1000}};
void routerPins(IOComponent*p,const std::vector<Region>&r) {
    circuit::memory::protocolPins(p,32);
    for(const auto&region:r)circuit::memory::protocolPins(p,32,region.prefix,true);
}
void buildRouter(IOComponent&root,ComponentBuilder&b,const std::vector<Region>&r,bool fetch) {
    Logic l(b);auto in=[&](const std::string&s){return root.getInputPinDynamic(s);};
    auto running=l.inv(in("RST"));auto busy=l.reg("BUSY",1,in("CLK"),in("RST"),l.bit(true));
    auto sent=l.reg("SENT",1,in("CLK"),in("RST"),l.bit(true));
    auto active=busy->getOutputPin("Q"),issued=sent->getOutputPin("Q");
    auto ready=l.land({running,l.inv(active)}),accept=l.land({ready,in("REQ_VALID")});
    auto known=b.addNewComponent<circuit::memory_detail::RequestKnownBits>("KNOWN_FIELDS",32);
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
    status=l.mux({l.any(V(address.begin(),address.begin()+2)),status,l.constant(3,1)});
    status=l.mux({l.land({write,l.inv(known->getOutputPin("MASK_KNOWN"))}),status,l.constant(3,6)});
    status=l.mux({l.inv(supported),status,l.constant(3,4)});
    status=l.mux({l.inv(known->getOutputPin("FIELDS_KNOWN")),status,l.constant(3,6)});
    auto error=l.any(status);
    auto st=l.reg("STATUS",3,in("CLK"),in("RST"),accept);l.drive(status,st->getInputPin<3>("D"));
    std::map<std::string,std::shared_ptr<IOComponent>> held;
    for(auto pair:{std::pair{"OP",3u},{"ADDR",32u},{"WRITE_DATA",32u},{"WRITE_MASK",4u}}) {
        auto reg=l.reg("HELD_"+std::string(pair.first),pair.second,in("CLK"),in("RST"),accept);
        l.wire(in("REQ_"+std::string(pair.first)),reg->getInputPinDynamic("D"));held[pair.first]=reg;
    }
    auto held_error=l.any(l.split(st->getOutputPin<3>("Q")));
    auto hv=l.split(held["ADDR"]->getOutputPin<32>("Q"));
    V responses,requests;V data=l.constant(32),response_status=l.split(st->getOutputPin<3>("Q"));
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
        for(auto pair:{std::pair{"OP",3u},{"WRITE_DATA",32u},{"WRITE_MASK",4u}})
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
    l.drive(data,root.getOutputPin<32>("RSP_READ_DATA"));l.drive(response_status,root.getOutputPin<3>("RSP_STATUS"));l.finish();
}
}
namespace circuit::device::detail {
P knownWord(Logic&l,ComponentBuilder&b,P data) {
    auto p=b.addNewComponent<circuit::memory_detail::RequestKnownBits>(l.id("DATA_KNOWN"),32);
    l.drive(l.constant(3),p->getInputPin<3>("OP"));l.drive(l.constant(4,15),p->getInputPin<4>("MASK"));
    l.wire(data,p->getInputPin<32>("ADDR"));return p->getOutputPin("FIELDS_KNOWN");
}
}
namespace circuit::families {
const ComponentFamily DeviceRegisterPort32{"device.register-port.32","DeviceRegisterPort32",portPins,structuralFactory<::DeviceRegisterPort32>(),
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return circuit::detail::createGateListPart(DeviceRegisterPort32,n,c);}};
const ComponentFamily BusErrorTarget32{"bus.error-target.32","BusErrorTarget32",[](IOComponent*p){memory::protocolPins(p,32);},structuralFactory<::BusErrorTarget32>()};
const ComponentFamily AddressRouter32{"bus.address-router.32","AddressRouter32",[](IOComponent*p){routerPins(p,regions);},[](const std::string& n,const std::shared_ptr<BuildContext>& c){
        return Component::createWithContext<::AddressRouter32>(c,n,parameter(c,"video_bytes",4096));},
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return circuit::detail::createGateListPart(AddressRouter32,n,c);}};
const ComponentFamily InstructionAccessGuard32{"bus.instruction-guard.32","InstructionAccessGuard32",[](IOComponent*p){routerPins(p,{{"MEM_",0,0x40000}});},structuralFactory<::InstructionAccessGuard32>(),
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return circuit::detail::createGateListPart(InstructionAccessGuard32,n,c);}};
}
DeviceRegisterPort32::DeviceRegisterPort32(std::string n):IOComponent(std::move(n),portPins){}
void DeviceRegisterPort32::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto clk=getInputPin("CLK"),rst=getInputPin("RST");auto running=l.inv(rst);
    auto pending=l.reg("PENDING",1,clk,rst,l.bit(true)),executed=l.reg("EXECUTED",1,clk,rst,l.bit(true));
    auto response=b.addByContract({"memory.shared.response-buffer.32","RESPONSE",{},{},{},{},circuit::Fidelity::Structural});
    for(auto pair:{std::pair{"CLK","CLK"},{"RST","RST"},{"RSP_READY","OUT_READY"}})l.wire(getInputPin(pair.first),response->getInputPin(pair.second));
    auto ready=l.land({running,l.land({l.inv(pending->getOutputPin("Q")),l.inv(response->getOutputPin("FULL"))})});
    auto accept=l.land({ready,getInputPin("REQ_VALID")});
    auto known=b.addNewComponent<circuit::memory_detail::RequestKnownBits>("KNOWN_FIELDS",32);
    for(auto pair:{std::pair{"REQ_OP","OP"},{"REQ_ADDR","ADDR"},{"REQ_WRITE_MASK","MASK"}})l.wire(getInputPinDynamic(pair.first),known->getInputPinDynamic(pair.second));
    auto op=l.split(getInputPin<3>("REQ_OP")),mask=l.split(getInputPin<4>("REQ_WRITE_MASK")),address=l.split(getInputPin<32>("REQ_ADDR"));
    auto read=l.equal({op,l.constant(3)}),write=l.equal({op,l.constant(3,1)});
    auto status=l.constant(3);
    status=l.mux({l.any(V(address.begin(),address.begin()+2)),status,l.constant(3,1)});
    auto badmask=l.land({write,l.lor({l.inv(known->getOutputPin("MASK_KNOWN")),l.land({l.any(mask),l.inv(l.all(mask))})})});
    status=l.mux({badmask,status,l.constant(3,6)});status=l.mux({l.inv(l.lor({read,write})),status,l.constant(3,4)});
    status=l.mux({l.inv(known->getOutputPin("FIELDS_KNOWN")),status,l.constant(3,6)});
    auto checked=l.reg("CHECKED_STATUS",3,clk,rst,accept);l.drive(status,checked->getInputPin<3>("D"));
    auto heldop=l.reg("OP",3,clk,rst,accept);l.wire(getInputPin<3>("REQ_OP"),heldop->getInputPin<3>("D"));
    auto heldaddr=l.reg("ADDRESS",32,clk,rst,accept);l.wire(getInputPin<32>("REQ_ADDR"),heldaddr->getInputPin<32>("D"));
    auto helddata=l.reg("DATA",32,clk,rst,accept);l.wire(getInputPin<32>("REQ_WRITE_DATA"),helddata->getInputPin<32>("D"));
    auto heldmask=l.reg("MASK",4,clk,rst,accept);l.wire(getInputPin<4>("REQ_WRITE_MASK"),heldmask->getInputPin<4>("D"));
    op=l.split(heldop->getOutputPin<3>("Q"));mask=l.split(heldmask->getOutputPin<4>("Q"));
    read=l.equal({op,l.constant(3)});write=l.equal({op,l.constant(3,1)});
    auto checkedstatus=l.split(checked->getOutputPin<3>("Q"));
    status=l.mux({l.any(checkedstatus),l.split(getInputPin<3>("BACK_STATUS")),checkedstatus});
    auto ok=l.inv(l.any(status)),action=l.land({write,l.land({l.any(mask),ok})});
    auto fire=l.land({running,l.land({pending->getOutputPin("Q"),l.land({l.inv(executed->getOutputPin("Q")),l.lor({l.inv(action),getInputPin("BACK_READY")})})})});
    l.wire(fire,response->getInputPin("IN_VALID"));l.drive(status,response->getInputPin<3>("IN_STATUS"));
    l.drive(l.mux({l.land({read,ok}),l.constant(32),l.split(getInputPin<32>("BACK_READ_DATA"))}),response->getInputPin<32>("IN_DATA"));
    auto take=l.land({response->getOutputPin("OUT_VALID"),getInputPin("RSP_READY")});
    l.wire(l.lor({accept,l.land({pending->getOutputPin("Q"),l.inv(take)})}),pending->getInputPin("D"));
    l.wire(l.land({l.inv(accept),l.land({l.inv(take),l.lor({fire,executed->getOutputPin("Q")})})}),executed->getInputPin("D"));
    l.out(ready,getOutputPin("REQ_READY"));
    for(auto pair:{std::pair{"OUT_VALID","RSP_VALID"},{"OUT_DATA","RSP_READ_DATA"},{"OUT_STATUS","RSP_STATUS"}})l.out(response->getOutputPinDynamic(pair.first),getOutputPinDynamic(pair.second));
    l.out(l.land({fire,l.land({read,ok})}),getOutputPin("READ_STROBE"));l.out(l.land({fire,action}),getOutputPin("WRITE_STROBE"));
    l.out(read,getOutputPin("IS_READ"));l.out(write,getOutputPin("IS_WRITE"));
    l.out(heldaddr->getOutputPin<32>("Q"),getOutputPin<32>("ADDRESS"));l.out(helddata->getOutputPin<32>("Q"),getOutputPin<32>("WRITE_DATA"));
    l.out(heldmask->getOutputPin<4>("Q"),getOutputPin<4>("WRITE_MASK"));l.finish();
}
BusErrorTarget32::BusErrorTarget32(std::string n):IOComponent(std::move(n),circuit::families::BusErrorTarget32.pinInitializer()){}
void BusErrorTarget32::buildInternals(ComponentBuilder&b) {
    Logic l(b);auto p=devicePort(*this,b,l);l.wire(l.bit(true),p->getInputPin("BACK_READY"));
    l.drive(l.constant(3,2),p->getInputPin<3>("BACK_STATUS"));l.drive(l.constant(32),p->getInputPin<32>("BACK_READ_DATA"));l.finish();
}
AddressRouter32::AddressRouter32(std::string n,size_t video_bytes):IOComponent(std::move(n),circuit::families::AddressRouter32.pinInitializer()),video_bytes_(video_bytes){
    if(!circuit::device::validVideoAperture(video_bytes))throw std::invalid_argument("Invalid video aperture");
}
void AddressRouter32::buildInternals(ComponentBuilder&b){auto configured=regions;configured.back().bytes=video_bytes_;buildRouter(*this,b,configured,false);}
InstructionAccessGuard32::InstructionAccessGuard32(std::string n):IOComponent(std::move(n),circuit::families::InstructionAccessGuard32.pinInitializer()){}
void InstructionAccessGuard32::buildInternals(ComponentBuilder&b){buildRouter(*this,b,{{"MEM_",0,0x40000}},true);}
