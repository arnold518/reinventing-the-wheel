#include "modules/device/DeviceBus.hpp"
#include "components/ComponentBuilder.tpp"
#include "DeviceGroups.hpp"
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
void routerPins(IOComponent*p,const std::vector<Region>&r,size_t width=32) {
    circuit::memory::protocolPins(p,width);
    for(const auto&region:r)circuit::memory::protocolPins(p,width,region.prefix,true);
}
void buildRouter(IOComponent& root,ComponentBuilder& b,const std::vector<Region>& regions,bool fetch,size_t width=32) {
    Logic l(b);const auto count=regions.size();const auto suffix="<"+std::to_string(width)+","+std::to_string(count)+">";
    Fields decode_out{{"STATUS",3},{"ERROR",1}};
    for(size_t i=0;i<count;++i)decode_out.emplace_back("SELECT_"+std::to_string(i),1);
    auto decode=group(b,"DECODE",std::string(fetch?"FetchAddressDecode":"DeviceAddressDecode")+suffix,
        {{"OP",3},{"ADDRESS",32},{"MASK",width/8}},decode_out,
        [regions,fetch,width](IOComponent& self,ComponentBuilder& gates) {
            Logic d(gates);auto known=gates.addNewComponent<circuit::memory_detail::RequestKnownBits>("KNOWN_FIELDS",width);
            for(auto pair:{std::pair{"OP","OP"},{"ADDRESS","ADDR"},{"MASK","MASK"}})d.wire(input(self,pair.first),known->getInputPinDynamic(pair.second));
            V selected;
            for(size_t i=0;i<regions.size();++i) {
                const auto region=regions[i];
                auto match=addressMatch(gates,region.prefix.substr(0,region.prefix.size()-1),"DeviceAddressWindow<"+std::to_string(std::countr_zero(region.bytes))+">",std::countr_zero(region.bytes),region.base);
                d.wire(input(self,"ADDRESS"),match->getInputPin<32>("ADDRESS"));selected.push_back(match->getOutputPin("MATCH"));
                d.wire(selected.back(),output(self,"SELECT_"+std::to_string(i)));
            }
            auto validation=group(gates,"VALIDATION",std::string(fetch?"FetchRequestValidation":"RouterRequestValidation")+"<"+std::to_string(width)+">",
                {{"OP",3},{"ADDRESS",32},{"MATCH",1},{"FIELDS_KNOWN",1},{"MASK_KNOWN",1}},{{"STATUS",3},{"ERROR",1}},
                [width,fetch](IOComponent& part,ComponentBuilder& builder) {
                    Logic v(builder);auto op=v.split(input(part,"OP")),address=v.split(input(part,"ADDRESS"));
                    auto read=v.equal({op,v.constant(3)}),write=v.equal({op,v.constant(3,1)});auto supported=fetch?read:v.lor({read,write});
                    auto status=statusMux(builder,v,"RANGE", {input(part,"MATCH"),v.constantWord(3,2),v.constantWord(3)});
                    status=statusMux(builder,v,"ALIGNMENT", {v.any(V(address.begin(),address.begin()+std::countr_zero(width/8))),status,v.constantWord(3,1)});
                    status=statusMux(builder,v,"MASK", {v.land({write,v.inv(input(part,"MASK_KNOWN"))}),status,v.constantWord(3,6)});
                    status=statusMux(builder,v,"OPERATION", {v.inv(supported),status,v.constantWord(3,4)});
                    status=statusMux(builder,v,"KNOWN", {v.inv(input(part,"FIELDS_KNOWN")),status,v.constantWord(3,6)});
                    v.wire(status,output(part,"STATUS"));v.wire(v.any(v.split(status)),output(part,"ERROR"));v.finish();
                });
            for(auto name:{"OP","ADDRESS"})d.wire(input(self,name),validation->getInputPinDynamic(name));
            d.wire(d.any(selected),validation->getInputPin("MATCH"));
            for(auto name:{"FIELDS_KNOWN","MASK_KNOWN"})d.wire(known->getOutputPin(name),validation->getInputPin(name));
            outputs(d,self,validation,{"STATUS","ERROR"});d.finish();
        });
    for(auto pair:{std::pair{"REQ_OP","OP"},{"REQ_ADDR","ADDRESS"},{"REQ_WRITE_MASK","MASK"}})l.wire(input(root,pair.first),decode->getInputPinDynamic(pair.second));
    Fields req_in{{"CLK",1},{"RST",1},{"ACCEPT",1},{"DECODED_STATUS",3},{"ERROR",1},{"REQ_OP",3},{"REQ_ADDR",32},{"REQ_WRITE_DATA",width},{"REQ_WRITE_MASK",width/8}};
    Fields req_out{{"STATUS",3},{"OP",3},{"ADDR",32},{"WRITE_DATA",width},{"WRITE_MASK",width/8}};
    for(size_t i=0;i<count;++i){req_in.emplace_back("SELECT_"+std::to_string(i),1);req_out.emplace_back("OWNER_"+std::to_string(i),1);}
    auto request=group(b,"REQUEST","DeviceRouterRequest"+suffix,req_in,req_out,
        [width,count](IOComponent& self,ComponentBuilder& gates) {
            Logic r(gates);auto accept=input(self,"ACCEPT");
            auto status=r.reg("STATUS",3,input(self,"CLK"),input(self,"RST"),accept);
            r.wire(input(self,"DECODED_STATUS"),status->getInputPin<3>("D"));r.wire(status->getOutputPin<3>("Q"),output(self,"STATUS"));
            for(auto field:Fields{{"OP",3},{"ADDR",32},{"WRITE_DATA",width},{"WRITE_MASK",width/8}}) {
                auto held=r.reg("HELD_"+field.first,field.second,input(self,"CLK"),input(self,"RST"),accept);
                r.wire(input(self,"REQ_"+field.first),held->getInputPinDynamic("D"));r.wire(held->getOutputPinDynamic("Q"),output(self,field.first));
            }
            for(size_t i=0;i<count;++i) {
                auto owner=r.reg("OWNER_"+std::to_string(i),1,input(self,"CLK"),input(self,"RST"),accept);
                r.wire(r.land({input(self,"SELECT_"+std::to_string(i)),r.inv(input(self,"ERROR"))}),owner->getInputPin("D"));
                r.wire(owner->getOutputPin("Q"),output(self,"OWNER_"+std::to_string(i)));
            }
            r.finish();
        });
    inputs(l,root,request,{"CLK","RST","REQ_OP","REQ_ADDR","REQ_WRITE_DATA","REQ_WRITE_MASK"});
    l.wire(decode->getOutputPin<3>("STATUS"),request->getInputPin<3>("DECODED_STATUS"));l.wire(decode->getOutputPin("ERROR"),request->getInputPin("ERROR"));
    for(size_t i=0;i<count;++i)l.wire(decode->getOutputPin("SELECT_"+std::to_string(i)),request->getInputPin("SELECT_"+std::to_string(i)));
    Fields ctrl_in{{"CLK",1},{"RST",1},{"REQ_VALID",1},{"RSP_READY",1},{"STATUS",3}};
    for(size_t i=0;i<count;++i){ctrl_in.emplace_back("GRANT_"+std::to_string(i),1);ctrl_in.emplace_back("RESPONSE_"+std::to_string(i),1);}
    auto control=group(b,"CONTROL","DeviceRouterControl<"+std::to_string(count)+">",ctrl_in,
        {{"REQ_READY",1},{"RSP_VALID",1},{"ACCEPT",1},{"ACTIVE",1},{"ISSUED",1}},
        [count](IOComponent& self,ComponentBuilder& gates) {
            Logic c(gates);auto running=c.inv(input(self,"RST"));auto busy=c.reg("BUSY",1,input(self,"CLK"),input(self,"RST"),c.bit(true));
            auto sent=c.reg("SENT",1,input(self,"CLK"),input(self,"RST"),c.bit(true));auto active=busy->getOutputPin("Q"),issued=sent->getOutputPin("Q");
            auto ready=c.land({running,c.inv(active)}),accept=c.land({ready,input(self,"REQ_VALID")});V grants,responses;
            for(size_t i=0;i<count;++i){grants.push_back(input(self,"GRANT_"+std::to_string(i)));responses.push_back(input(self,"RESPONSE_"+std::to_string(i)));}
            auto valid=c.land({running,c.land({active,c.lor({c.any(c.split(input(self,"STATUS"))),c.any(responses)})})});auto done=c.land({valid,input(self,"RSP_READY")});
            c.wire(c.lor({accept,c.land({active,c.inv(done)})}),busy->getInputPin("D"));
            c.wire(c.land({active,c.land({c.inv(done),c.lor({issued,c.any(grants)})})}),sent->getInputPin("D"));
            for(auto pair:std::array<std::pair<const char*,P>,5>{{{"REQ_READY",ready},{"RSP_VALID",valid},{"ACCEPT",accept},{"ACTIVE",active},{"ISSUED",issued}}})c.wire(pair.second,output(self,pair.first));
            c.finish();
        });
    inputs(l,root,control,{"CLK","RST","REQ_VALID","RSP_READY"});outputs(l,root,control,{"REQ_READY","RSP_VALID"});
    l.wire(request->getOutputPin<3>("STATUS"),control->getInputPin<3>("STATUS"));l.wire(control->getOutputPin("ACCEPT"),request->getInputPin("ACCEPT"));
    Fields response_in{{"HELD_STATUS",3}};
    for(size_t i=0;i<count;++i){response_in.emplace_back("OWNER_"+std::to_string(i),1);response_in.emplace_back("DATA_"+std::to_string(i),width);response_in.emplace_back("STATUS_"+std::to_string(i),3);}
    auto response=group(b,"RESPONSE_SELECT","DeviceRouterResponse"+suffix,response_in,{{"DATA",width},{"STATUS",3}},
        [width,count](IOComponent& self,ComponentBuilder& gates) {
            Logic r(gates);auto data=r.constantWord(width),status=input(self,"HELD_STATUS");
            for(size_t i=0;i<count;++i) {
                const auto name=std::to_string(i);auto own=input(self,"OWNER_"+name);
                data=wordMux(gates,r,"DATA_"+name, {own,data,input(self,"DATA_"+name)});
                status=statusMux(gates,r,"STATUS_"+name, {own,status,input(self,"STATUS_"+name)});
            }
            r.wire(data,output(self,"DATA"));r.wire(status,output(self,"STATUS"));r.finish();
        });
    l.wire(request->getOutputPin<3>("STATUS"),response->getInputPin<3>("HELD_STATUS"));
    l.wire(response->getOutputPinDynamic("DATA"),output(root,"RSP_READ_DATA"));l.wire(response->getOutputPin<3>("STATUS"),output(root,"RSP_STATUS"));
    for(size_t i=0;i<count;++i) {
        const auto region=regions[i];const auto index=std::to_string(i);
        auto channel=group(b,"ROUTE_"+region.prefix.substr(0,region.prefix.size()-1),"DeviceRouteChannel<"+std::to_string(width)+","+(region.local?std::to_string(std::countr_zero(region.bytes)):std::string("full"))+">",
            {{"CLK",1},{"RST",1},{"ACTIVE",1},{"ISSUED",1},{"OWNER",1},{"UPSTREAM_READY",1},{"REQ_READY",1},{"RSP_VALID",1},{"OP",3},{"ADDR",32},{"WRITE_DATA",width},{"WRITE_MASK",width/8}},
            {{"TARGET_CLK",1},{"TARGET_RST",1},{"REQ_VALID",1},{"RSP_READY",1},{"REQ_OP",3},{"REQ_ADDR",32},{"REQ_WRITE_DATA",width},{"REQ_WRITE_MASK",width/8},{"GRANT",1},{"RESPONSE",1}},
            [region](IOComponent& self,ComponentBuilder& gates) {
                Logic c(gates);auto active=input(self,"ACTIVE"),own=input(self,"OWNER"),issued=input(self,"ISSUED");auto running=c.inv(input(self,"RST"));
                auto request=c.land({running,c.land({active,c.land({own,c.inv(issued)})})});auto grant=c.land({request,input(self,"REQ_READY")});
                auto pending=c.land({active,c.land({own,c.lor({issued,grant})})});
                c.wire(request,output(self,"REQ_VALID"));c.wire(grant,output(self,"GRANT"));
                c.wire(c.land({pending,input(self,"RSP_VALID")}),output(self,"RESPONSE"));
                c.wire(c.land({running,c.land({pending,input(self,"UPSTREAM_READY")})}),output(self,"RSP_READY"));
                for(auto name:{"CLK","RST"})c.out(input(self,name),output(self,"TARGET_"+std::string(name)));
                for(auto name:{"OP","WRITE_DATA","WRITE_MASK"})c.out(input(self,name),output(self,"REQ_"+std::string(name)));
                if(region.local){
                    auto address=c.split(input(self,"ADDR")),offset=c.constant(32);for(size_t j=0;j<std::countr_zero(region.bytes);++j)offset[j]=address[j];
                    c.drive(offset,output(self,"REQ_ADDR"));
                } else c.out(input(self,"ADDR"),output(self,"REQ_ADDR"));
                c.finish();
            });
        inputs(l,root,channel,{"CLK","RST"});l.wire(input(root,"RSP_READY"),channel->getInputPin("UPSTREAM_READY"));
        for(auto name:{"ACTIVE","ISSUED"})l.wire(control->getOutputPin(name),channel->getInputPin(name));
        l.wire(request->getOutputPin("OWNER_"+index),channel->getInputPin("OWNER"));l.wire(request->getOutputPin("OWNER_"+index),response->getInputPin("OWNER_"+index));
        for(auto name:{"OP","ADDR","WRITE_DATA","WRITE_MASK"})l.wire(request->getOutputPinDynamic(name),channel->getInputPinDynamic(name));
        for(auto name:{"REQ_READY","RSP_VALID"})l.wire(input(root,region.prefix+name),channel->getInputPin(name));
        for(auto name:{"REQ_VALID","REQ_OP","REQ_ADDR","REQ_WRITE_DATA","REQ_WRITE_MASK","RSP_READY"})l.wire(channel->getOutputPinDynamic(name),output(root,region.prefix+name));
        for(auto name:{"CLK","RST"})l.wire(channel->getOutputPin("TARGET_"+std::string(name)),output(root,region.prefix+name));
        l.wire(channel->getOutputPin("GRANT"),control->getInputPin("GRANT_"+index));l.wire(channel->getOutputPin("RESPONSE"),control->getInputPin("RESPONSE_"+index));
        l.wire(input(root,region.prefix+"RSP_READ_DATA"),response->getInputPinDynamic("DATA_"+index));
        l.wire(input(root,region.prefix+"RSP_STATUS"),response->getInputPin<3>("STATUS_"+index));
    }
    l.finish();
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
    Logic l(b);const auto suffix="<"+std::to_string(width)+">";
    auto request=group(b,"REQUEST","DeviceRegisterRequest"+suffix,
        {{"CLK",1},{"RST",1},{"ACCEPT",1},{"REQ_OP",3},{"REQ_ADDR",32},{"REQ_WRITE_DATA",width},{"REQ_WRITE_MASK",width/8}},
        {{"OP",3},{"ADDRESS",32},{"DATA",width},{"MASK",width/8},{"STATUS",3}},
        [width](IOComponent& self,ComponentBuilder& gates) {
            Logic r(gates);auto validation=group(gates,"VALIDATION","DeviceRegisterValidation<"+std::to_string(width)+">",
                {{"OP",3},{"ADDR",32},{"MASK",width/8}},{{"STATUS",3}},
                [width](IOComponent& part,ComponentBuilder& builder) {
                    Logic v(builder);auto known=builder.addNewComponent<circuit::memory_detail::RequestKnownBits>("KNOWN_FIELDS",width);
                    for(auto name:{"OP","ADDR","MASK"})v.wire(input(part,name),known->getInputPinDynamic(name));
                    auto op=v.split(input(part,"OP")),mask=v.split(input(part,"MASK")),address=v.split(input(part,"ADDR"));
                    auto read=v.equal({op,v.constant(3)}),write=v.equal({op,v.constant(3,1)});
                    auto status=statusMux(builder,v,"ALIGNMENT", {v.any(V(address.begin(),address.begin()+std::countr_zero(width/8))),v.constantWord(3),v.constantWord(3,1)});
                    auto badmask=v.land({write,v.lor({v.inv(known->getOutputPin("MASK_KNOWN")),v.land({v.any(mask),v.inv(v.all(mask))})})});
                    status=statusMux(builder,v,"MASK", {badmask,status,v.constantWord(3,6)});
                    status=statusMux(builder,v,"OPERATION", {v.inv(v.lor({read,write})),status,v.constantWord(3,4)});
                    status=statusMux(builder,v,"KNOWN", {v.inv(known->getOutputPin("FIELDS_KNOWN")),status,v.constantWord(3,6)});v.wire(status,output(part,"STATUS"));v.finish();
                });
            for(auto pair:{std::pair{"REQ_OP","OP"},{"REQ_ADDR","ADDR"},{"REQ_WRITE_MASK","MASK"}})r.wire(input(self,pair.first),validation->getInputPinDynamic(pair.second));
            auto checked=r.reg("CHECKED_STATUS",3,input(self,"CLK"),input(self,"RST"),input(self,"ACCEPT"));
            r.wire(validation->getOutputPin<3>("STATUS"),checked->getInputPin<3>("D"));r.wire(checked->getOutputPin<3>("Q"),output(self,"STATUS"));
            for(auto field:Fields{{"OP",3},{"ADDRESS",32},{"DATA",width},{"MASK",width/8}}) {
                const auto original=field.first=="ADDRESS"?"ADDR":field.first=="DATA"?"WRITE_DATA":field.first=="MASK"?"WRITE_MASK":"OP";
                auto held=r.reg(field.first,field.second,input(self,"CLK"),input(self,"RST"),input(self,"ACCEPT"));
                r.wire(input(self,"REQ_"+std::string(original)),held->getInputPinDynamic("D"));r.wire(held->getOutputPinDynamic("Q"),output(self,field.first));
            }
            r.finish();
        });
    inputs(l,root,request,{"CLK","RST","REQ_OP","REQ_ADDR","REQ_WRITE_DATA","REQ_WRITE_MASK"});
    auto response=b.addByContract({"memory.shared.response-buffer."+std::to_string(width),"RESPONSE",{},{},{},{},circuit::Fidelity::Structural});
    for(auto pair:{std::pair{"CLK","CLK"},{"RST","RST"},{"RSP_READY","OUT_READY"}})l.wire(input(root,pair.first),response->getInputPin(pair.second));
    auto control=group(b,"CONTROL","DeviceRegisterControl",{{"CLK",1},{"RST",1},{"REQ_VALID",1},{"RSP_READY",1},{"FULL",1},{"VALID",1},{"ACTION",1},{"BACK_READY",1}},
        {{"REQ_READY",1},{"ACCEPT",1},{"FIRE",1}},
        [](IOComponent& self,ComponentBuilder& gates) {
            Logic c(gates);auto running=c.inv(input(self,"RST"));auto pending=c.reg("PENDING",1,input(self,"CLK"),input(self,"RST"),c.bit(true));
            auto executed=c.reg("EXECUTED",1,input(self,"CLK"),input(self,"RST"),c.bit(true));
            auto ready=c.land({running,c.land({c.inv(pending->getOutputPin("Q")),c.inv(input(self,"FULL"))})}),accept=c.land({ready,input(self,"REQ_VALID")});
            auto fire=c.land({running,c.land({pending->getOutputPin("Q"),c.land({c.inv(executed->getOutputPin("Q")),c.lor({c.inv(input(self,"ACTION")),input(self,"BACK_READY")})})})});
            auto take=c.land({input(self,"VALID"),input(self,"RSP_READY")});
            c.wire(c.lor({accept,c.land({pending->getOutputPin("Q"),c.inv(take)})}),pending->getInputPin("D"));
            c.wire(c.land({c.inv(accept),c.land({c.inv(take),c.lor({fire,executed->getOutputPin("Q")})})}),executed->getInputPin("D"));
            c.wire(ready,output(self,"REQ_READY"));c.wire(accept,output(self,"ACCEPT"));c.wire(fire,output(self,"FIRE"));c.finish();
        });
    inputs(l,root,control,{"CLK","RST","REQ_VALID","RSP_READY","BACK_READY"});outputs(l,root,control,{"REQ_READY"});
    l.wire(response->getOutputPin("FULL"),control->getInputPin("FULL"));l.wire(response->getOutputPin("OUT_VALID"),control->getInputPin("VALID"));
    l.wire(control->getOutputPin("ACCEPT"),request->getInputPin("ACCEPT"));
    auto execution=group(b,"EXECUTE","DeviceRegisterExecution"+suffix,{{"OP",3},{"MASK",width/8},{"CHECKED_STATUS",3},{"BACK_STATUS",3},{"BACK_READ_DATA",width},{"FIRE",1}},
        {{"STATUS",3},{"DATA",width},{"ACTION",1},{"IS_READ",1},{"IS_WRITE",1},{"READ_STROBE",1},{"WRITE_STROBE",1}},
        [width](IOComponent& self,ComponentBuilder& gates) {
            Logic e(gates);auto op=e.split(input(self,"OP")),mask=e.split(input(self,"MASK")),checked=e.split(input(self,"CHECKED_STATUS"));
            auto read=e.equal({op,e.constant(3)}),write=e.equal({op,e.constant(3,1)});
            auto status=statusMux(gates,e,"STATUS", {e.any(checked),input(self,"BACK_STATUS"),input(self,"CHECKED_STATUS")});
            auto ok=e.inv(e.any(e.split(status))),action=e.land({write,e.land({e.any(mask),ok})});
            e.wire(status,output(self,"STATUS"));e.wire(action,output(self,"ACTION"));e.wire(read,output(self,"IS_READ"));e.wire(write,output(self,"IS_WRITE"));
            e.wire(wordMux(gates,e,"READ_DATA", {e.land({read,ok}),e.constantWord(width),input(self,"BACK_READ_DATA")}),output(self,"DATA"));
            e.wire(e.land({input(self,"FIRE"),e.land({read,ok})}),output(self,"READ_STROBE"));e.wire(e.land({input(self,"FIRE"),action}),output(self,"WRITE_STROBE"));e.finish();
        });
    inputs(l,root,execution,{"BACK_STATUS","BACK_READ_DATA"});
    l.wire(request->getOutputPin<3>("OP"),execution->getInputPin<3>("OP"));l.wire(request->getOutputPinDynamic("MASK"),execution->getInputPinDynamic("MASK"));
    l.wire(request->getOutputPin<3>("STATUS"),execution->getInputPin<3>("CHECKED_STATUS"));l.wire(control->getOutputPin("FIRE"),execution->getInputPin("FIRE"));
    l.wire(execution->getOutputPin("ACTION"),control->getInputPin("ACTION"));l.wire(control->getOutputPin("FIRE"),response->getInputPin("IN_VALID"));
    l.wire(execution->getOutputPin<3>("STATUS"),response->getInputPin<3>("IN_STATUS"));l.wire(execution->getOutputPinDynamic("DATA"),response->getInputPinDynamic("IN_DATA"));
    outputs(l,root,execution,{"READ_STROBE","WRITE_STROBE","IS_READ","IS_WRITE"});
    for(auto pair:{std::pair{"ADDRESS","ADDRESS"},{"DATA","WRITE_DATA"},{"MASK","WRITE_MASK"}})l.wire(request->getOutputPinDynamic(pair.first),output(root,pair.second));
    for(auto pair:{std::pair{"OUT_VALID","RSP_VALID"},{"OUT_DATA","RSP_READ_DATA"},{"OUT_STATUS","RSP_STATUS"}})l.wire(response->getOutputPinDynamic(pair.first),output(root,pair.second));
    l.finish();
}
}
namespace circuit::device {
namespace {
void wordBusPins(IOComponent* p,BusUnit,size_t w){
    if(w!=8 && w!=16 && w!=32)throw std::invalid_argument("Bus data width must be 8, 16 or 32");
    portPins(p,w);
}
std::string busName(BusUnit){return "DeviceRegisterPort";}
}
std::string busId(BusUnit,size_t w){return "device.word.register-port."+std::to_string(w);}

WordBusComponent::WordBusComponent(std::string n,BusUnit u,size_t width)
    :IOComponent(std::move(n),[u,width](IOComponent* p){wordBusPins(p,u,width);}),unit_(u),width_(width),type_(busName(u)+"<"+std::to_string(width)+">"){}
void WordBusComponent::buildInternals(ComponentBuilder& b){buildPort(*this,b,width_);}

const ComponentFamily& busFamily(BusUnit u,size_t width){
    if(width!=8 && width!=16 && width!=32)throw std::invalid_argument("Invalid bus width");
    struct Entry{std::string id,type;ComponentFamily f;
        Entry(BusUnit u,size_t w):id(busId(u,w)),type(busName(u)+"<"+std::to_string(w)+">"),
            f(id,type,[u,w](IOComponent*p){wordBusPins(p,u,w);},[u,w](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<WordBusComponent>(c,n,u,w);},
                w==32?FamilyFactory([u,w](const std::string& n,const std::shared_ptr<BuildContext>& c){return circuit::detail::createGateListPart(busFamily(u,w),n,c);}):FamilyFactory{}){}};
    static std::map<std::pair<BusUnit,size_t>,std::unique_ptr<Entry>> entries;auto& e=entries[{u,width}];if(!e)e=std::make_unique<Entry>(u,width);return e->f;
}
void registerWordBusComponents(ComponentCatalog& catalog) {
    for(size_t width:{8u,16u,32u})for(auto unit:{BusUnit::RegisterPort}) {
        const auto& f=busFamily(unit,width);
        const std::string owner=width==32?busName(unit)+"Width32Test":"WidthComponentsTest";
        const ParameterPredicate supports=[](const ParameterMap& p){return p.empty();};
        registration::registerStructural(catalog,f,owner,"clock-boundary",supports);
        if(f.supports(Fidelity::Behavioral))registration::registerBehavioral(catalog,f,owner,busName(unit)+"Width32ScenarioTest",supports);
    }
}
}
namespace circuit::device {
void addressRouterPins(IOComponent* p,const std::vector<AddressRegion>& r,size_t width){routerPins(p,r,width);}
void buildAddressRouter(IOComponent& root,ComponentBuilder& b,const std::vector<AddressRegion>& r,bool fetch,size_t width){buildRouter(root,b,r,fetch,width);}
}
