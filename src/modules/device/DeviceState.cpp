#include "components/ComponentBuilder.tpp"
#include "DeviceGroups.hpp"
#include "modules/device/RegisterTable.hpp"
#include "modules/basic/Decoder.hpp"
#include "modules/basic/WordGate.hpp"
#include "components/GateListPartBehavioral.hpp"
#include "components/selection/FamilyFactories.hpp"
#include "components/selection/FamilyRegistration.hpp"
using namespace circuit::device::detail;
namespace {
void counterPins(IOComponent*p,size_t width) {
    if(width!=4&&width!=8&&width!=16&&width!=32)throw std::invalid_argument("Counter supports 4, 8, 16 and 32 bits");
    for(auto s:{"CLK","RST","ENABLE"})p->addPin(s,PinType::INPUT);
    p->addPinDynamic("COUNT",PinType::OUTPUT,width);p->addPin("TICK",PinType::OUTPUT);
}
void fifoPins(IOComponent*p,size_t width) {
    if(width!=8&&width!=16&&width!=32)throw std::invalid_argument("FIFO supports 8, 16 and 32 bits");
    for(auto s:{"CLK","RST","PUSH","POP","CLEAR_OVERFLOW"})p->addPin(s,PinType::INPUT);
    p->addPinDynamic("PUSH_DATA",PinType::INPUT,width);p->addPinDynamic("DATA",PinType::OUTPUT,width);
    for(auto s:{"PUSH_READY","VALID","FULL","OVERFLOW"})p->addPin(s,PinType::OUTPUT);
    p->addPin<3>("COUNT",PinType::OUTPUT);
}
void consolePins(IOComponent*p){
    circuit::memory::protocolPins(p,32);p->addPin("TX_READY",PinType::INPUT);
    p->addPin("TX_VALID",PinType::OUTPUT);p->addPin<8>("TX_DATA",PinType::OUTPUT);
}
void buttonPins(IOComponent*p){
    circuit::memory::protocolPins(p,32);
    for(auto s:{"INPUT_VALID","INPUT_DOWN"})p->addPin(s,PinType::INPUT);
    p->addPin<5>("INPUT_BUTTON",PinType::INPUT);
}
}
namespace circuit::families {
const ComponentFamily& clockedCounter(size_t w) {
    static const ComponentFamily small{"device.counter.4","ClockedCounter<4>",[](IOComponent*p){counterPins(p,4);},
      [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::ClockedCounter>(c,n,4,circuit::device::detail::parameter(c,"divisor",1));},
      [](const std::string&n,const std::shared_ptr<BuildContext>&c){return circuit::detail::createGateListPart(clockedCounter(4),n,c);}};
    static const ComponentFamily word{"device.counter.32","ClockedCounter<32>",[](IOComponent*p){counterPins(p,32);},
      [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::ClockedCounter>(c,n,32,circuit::device::detail::parameter(c,"divisor",1));},
      [](const std::string&n,const std::shared_ptr<BuildContext>&c){return circuit::detail::createGateListPart(clockedCounter(32),n,c);}};
    static const ComponentFamily byte{"device.counter.8","ClockedCounter<8>",[](IOComponent*p){counterPins(p,8);},
      [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::ClockedCounter>(c,n,8,parameter(c,"divisor",1));}};
    static const ComponentFamily half{"device.counter.16","ClockedCounter<16>",[](IOComponent*p){counterPins(p,16);},
      [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::ClockedCounter>(c,n,16,parameter(c,"divisor",1));}};
    if(w==8)return byte;if(w==16)return half;
    if(w==4)return small;if(w==32)return word;throw std::invalid_argument("Invalid counter width");
}
const ComponentFamily& synchronousFifo(size_t w) {
    static const ComponentFamily byte{"device.fifo.8","SynchronousFifo<8>",[](IOComponent*p){fifoPins(p,8);},
      [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::SynchronousFifo>(c,n,8,parameter(c,"depth",4));},
      [](const std::string&n,const std::shared_ptr<BuildContext>&c){return circuit::detail::createGateListPart(synchronousFifo(8),n,c);}};
    static const ComponentFamily word{"device.fifo.32","SynchronousFifo<32>",[](IOComponent*p){fifoPins(p,32);},
      [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::SynchronousFifo>(c,n,32,parameter(c,"depth",4));},
      [](const std::string&n,const std::shared_ptr<BuildContext>&c){return circuit::detail::createGateListPart(synchronousFifo(32),n,c);}};
    static const ComponentFamily half{"device.fifo.16","SynchronousFifo<16>",[](IOComponent*p){fifoPins(p,16);},
      [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::SynchronousFifo>(c,n,16,parameter(c,"depth",4));}};
    if(w==16)return half;
    if(w==8)return byte;if(w==32)return word;throw std::invalid_argument("Invalid FIFO width");
}
const ComponentFamily ConsoleDevice{"device.console","ConsoleDevice",consolePins,
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::ConsoleDevice>(c,n,parameter(c,"fifo_depth",4));},
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<DeviceBehavioral<::ConsoleDevice>>(c,n,parameter(c,"fifo_depth",4));}};
const ComponentFamily TimerDevice{"device.timer","TimerDevice",[](IOComponent*p){memory::protocolPins(p,32);},
    [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::TimerDevice>(c,n,
        circuit::device::detail::parameter(c,"divisor",1),circuit::device::detail::parameter(c,"frequency_hz",0));},
    [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<DeviceBehavioral<::TimerDevice>>(c,n,
        circuit::device::detail::parameter(c,"divisor",1),circuit::device::detail::parameter(c,"frequency_hz",0));}};
const ComponentFamily ButtonDevice{"device.buttons","ButtonDevice",buttonPins,
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::ButtonDevice>(c,n,parameter(c,"fifo_depth",4));},
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<DeviceBehavioral<::ButtonDevice>>(c,n,parameter(c,"fifo_depth",4));}};
}
ClockedCounter::ClockedCounter(std::string n,size_t w,uint32_t divisor):IOComponent(std::move(n),[w](IOComponent*p){counterPins(p,w);}),
    width_(w),divisor_(divisor),type_("ClockedCounter<"+std::to_string(w)+">"){if(!divisor)throw std::invalid_argument("Counter divisor must be positive");}
void ClockedCounter::buildInternals(ComponentBuilder& b) {
    Logic l(b);
    auto tick = group(b,"TICK_CONTROL",divisor_==1?"ClockTickEnable":"ClockTickDivider",
        {{"CLK",1},{"RST",1},{"ENABLE",1}},{{"TICK",1}},
        [divisor=divisor_](IOComponent& self,ComponentBuilder& gates) {
            Logic logic(gates);
            auto enabled=logic.land({input(self,"ENABLE"),logic.inv(input(self,"RST"))});
            if(divisor!=1) {
                auto phase=logic.reg("PHASE",32,input(self,"CLK"),input(self,"RST"),input(self,"ENABLE"));
                auto match=group(gates,"TERMINAL","ClockDividerTerminal",{{"PHASE",32}},{{"LAST",1}},
                    [divisor](IOComponent& part,ComponentBuilder& builder) {
                        Logic m(builder);m.wire(m.equal({m.split(input(part,"PHASE")),m.constant(32,divisor-1)}),output(part,"LAST"));m.finish();
                    });
                logic.wire(phase->getOutputPin<32>("Q"),match->getInputPin<32>("PHASE"));
                auto next=group(gates,"ADVANCE","ClockDividerAdvance",{{"PHASE",32},{"LAST",1}},{{"NEXT",32}},
                    [](IOComponent& part,ComponentBuilder& builder) {
                        Logic n(builder);auto inc=n.add({n.split(input(part,"PHASE")),n.constant(32,1),n.bit(false)}).first;
                        n.wire(wordMux(builder,n,"WRAP", {input(part,"LAST"),n.join(inc),n.constantWord(32)}),output(part,"NEXT"));n.finish();
                    });
                logic.wire(phase->getOutputPin<32>("Q"),next->getInputPin<32>("PHASE"));
                logic.wire(match->getOutputPin("LAST"),next->getInputPin("LAST"));
                logic.wire(next->getOutputPin<32>("NEXT"),phase->getInputPin<32>("D"));
                enabled=logic.land({enabled,match->getOutputPin("LAST")});
            }
            logic.wire(enabled,output(self,"TICK"));logic.finish();
        });
    inputs(l,*this,tick,{"CLK","RST","ENABLE"});
    auto count=l.reg("COUNT_STATE",width_,getInputPin("CLK"),getInputPin("RST"),tick->getOutputPin("TICK"));
    auto increment=group(b,"INCREMENT","CounterIncrement<"+std::to_string(width_)+">",{{"COUNT",width_}},{{"NEXT",width_}},
        [width=width_](IOComponent& self,ComponentBuilder& gates) {
            Logic logic(gates);logic.drive(logic.add({logic.split(input(self,"COUNT")),logic.constant(width,1),logic.bit(false)}).first,output(self,"NEXT"));logic.finish();
        });
    l.wire(count->getOutputPinDynamic("Q"),increment->getInputPinDynamic("COUNT"));
    l.wire(increment->getOutputPinDynamic("NEXT"),count->getInputPinDynamic("D"));
    l.wire(count->getOutputPinDynamic("Q"),getOutputPinDynamic("COUNT"));
    l.wire(tick->getOutputPin("TICK"),getOutputPin("TICK"));l.finish();
}
SynchronousFifo::SynchronousFifo(std::string n,size_t w,size_t depth):IOComponent(std::move(n),[w](IOComponent*p){fifoPins(p,w);}),
    width_(w),depth_(depth),type_("SynchronousFifo<"+std::to_string(w)+">"){
    if(!circuit::device::validFifoDepth(depth))throw std::invalid_argument("FIFO depth must be in 1..7 (three-bit COUNT)");
}
void SynchronousFifo::buildInternals(ComponentBuilder& b) {
    Logic l(b);const size_t pointer_width=depth_<=4?2:3;
    auto control=group(b,"CONTROL","FifoOccupancy",{{"CLK",1},{"RST",1},{"PUSH",1},{"POP",1},{"CLEAR_OVERFLOW",1}},
        {{"COUNT",3},{"FULL",1},{"VALID",1},{"PUSH_READY",1},{"PUSH_ACCEPT",1},{"POP_ACCEPT",1},{"OVERFLOW",1}},
        [depth=depth_](IOComponent& self,ComponentBuilder& gates) {
            Logic c(gates);auto count=c.reg("COUNT_STATE",3,input(self,"CLK"),input(self,"RST"),c.bit(true));
            auto value=c.split(count->getOutputPin<3>("Q"));
            auto empty=c.equal({value,c.constant(3)}),full=c.equal({value,c.constant(3,depth)});
            auto ready=c.land({c.inv(input(self,"RST")),c.inv(full)}),valid=c.land({c.inv(input(self,"RST")),c.inv(empty)});
            auto push=c.land({input(self,"PUSH"),ready}),pop=c.land({input(self,"POP"),valid});
            auto inc=c.add({value,c.constant(3),push}).first;
            c.drive(c.sub({inc,{pop,c.bit(false),c.bit(false)}}).first,count->getInputPin<3>("D"));
            auto overflow=c.reg("OVERFLOW_STATE",1,input(self,"CLK"),input(self,"RST"),c.bit(true));
            auto dropped=c.land({input(self,"PUSH"),full});
            c.wire(c.lor({dropped,c.land({overflow->getOutputPin("Q"),c.inv(input(self,"CLEAR_OVERFLOW"))})}),overflow->getInputPin("D"));
            for(auto pair:std::array<std::pair<const char*,P>,7>{{{"FULL",full},{"VALID",valid},{"PUSH_READY",ready},{"PUSH_ACCEPT",push},{"POP_ACCEPT",pop},{"OVERFLOW",overflow->getOutputPin("Q")},{"COUNT",count->getOutputPin<3>("Q")}}})
                c.wire(pair.second,output(self,pair.first));
            c.finish();
        });
    inputs(l,*this,control,{"CLK","RST","PUSH","POP","CLEAR_OVERFLOW"});
    outputs(l,*this,control,{"COUNT","FULL","VALID","PUSH_READY","OVERFLOW"});
    auto pointers=group(b,"POINTERS","FifoPointers<"+std::to_string(depth_)+">",
        {{"CLK",1},{"RST",1},{"PUSH_ACCEPT",1},{"POP_ACCEPT",1}},{{"HEAD",pointer_width},{"TAIL",pointer_width}},
        [depth=depth_,pointer_width](IOComponent& self,ComponentBuilder& gates) {
            Logic p(gates);
            for(auto name:{"HEAD","TAIL"}) {
                auto ptr=p.reg(name,pointer_width,input(self,"CLK"),input(self,"RST"),input(self,std::string(name)=="HEAD"?"POP_ACCEPT":"PUSH_ACCEPT"));
                auto value=p.split(ptr->getOutputPinDynamic("Q"));auto next=p.add({value,p.constant(pointer_width,1),p.bit(false)}).first;
                if(depth!=4)next=p.mux({p.equal({value,p.constant(pointer_width,depth-1)}),next,p.constant(pointer_width)});
                p.drive(next,ptr->getInputPinDynamic("D"));p.wire(ptr->getOutputPinDynamic("Q"),output(self,name));
            }
            p.finish();
        });
    inputs(l,*this,pointers,{"CLK","RST"});
    for(auto name:{"PUSH_ACCEPT","POP_ACCEPT"})l.wire(control->getOutputPin(name),pointers->getInputPin(name));
    auto storage=group(b,"STORAGE","FifoStorage<"+std::to_string(width_)+","+std::to_string(depth_)+">",
        {{"CLK",1},{"RST",1},{"PUSH_ACCEPT",1},{"HEAD",pointer_width},{"TAIL",pointer_width},{"PUSH_DATA",width_},{"VALID",1}},{{"DATA",width_}},
        [width=width_,depth=depth_,pointer_width](IOComponent& self,ComponentBuilder& gates) {
            Logic s(gates);Fields enables;for(size_t i=0;i<depth;++i)enables.emplace_back("SLOT_"+std::to_string(i),1);
            auto decode=group(gates,"WRITE_ENABLES","FifoWriteDecode<"+std::to_string(depth)+">",{{"TAIL",pointer_width},{"PUSH",1}},enables,
                [depth](IOComponent& part,ComponentBuilder& builder) {
                    Logic d(builder);auto tail=d.split(input(part,"TAIL"));
                    for(size_t i=0;i<depth;++i)d.wire(d.land({input(part,"PUSH"),d.equal({tail,d.constant(tail.size(),i)})}),output(part,"SLOT_"+std::to_string(i)));
                    d.finish();
                });
            s.wire(input(self,"TAIL"),decode->getInputPinDynamic("TAIL"));s.wire(input(self,"PUSH_ACCEPT"),decode->getInputPin("PUSH"));
            Fields fields{{"HEAD",pointer_width},{"VALID",1}};
            for(size_t i=0;i<depth;++i)fields.emplace_back("SLOT_"+std::to_string(i),width);
            auto select=group(gates,"READ_SELECT","FifoReadSelect<"+std::to_string(width)+","+std::to_string(depth)+">",fields,{{"DATA",width}},
                [width,depth](IOComponent& part,ComponentBuilder& builder) {
                    Logic r(builder);auto head=r.split(input(part,"HEAD"));auto zero=r.constantWord(width);auto data=zero;
                    for(size_t i=0;i<depth;++i)data=wordMux(builder,r,"SLOT_"+std::to_string(i), {r.equal({head,r.constant(head.size(),i)}),data,input(part,"SLOT_"+std::to_string(i))});
                    r.wire(wordMux(builder,r,"VALID_DATA", {input(part,"VALID"),zero,data}),output(part,"DATA"));r.finish();
                });
            for(auto name:{"HEAD","VALID"})s.wire(input(self,name),select->getInputPinDynamic(name));
            for(size_t i=0;i<depth;++i) {
                auto name="SLOT_"+std::to_string(i);auto slot=s.reg(name,width,input(self,"CLK"),input(self,"RST"),decode->getOutputPin(name));
                s.wire(input(self,"PUSH_DATA"),slot->getInputPinDynamic("D"));s.wire(slot->getOutputPinDynamic("Q"),select->getInputPinDynamic(name));
            }
            s.wire(select->getOutputPinDynamic("DATA"),output(self,"DATA"));s.finish();
        });
    inputs(l,*this,storage,{"CLK","RST","PUSH_DATA"});
    for(auto name:{"PUSH_ACCEPT","VALID"})l.wire(control->getOutputPin(name),storage->getInputPin(name));
    for(auto name:{"HEAD","TAIL"})l.wire(pointers->getOutputPinDynamic(name),storage->getInputPinDynamic(name));
    outputs(l,*this,storage,{"DATA"});l.finish();
}
ConsoleDevice::ConsoleDevice(std::string n,size_t fifo_depth):IOComponent(std::move(n),consolePins),fifo_depth_(fifo_depth){
    if(!circuit::device::validFifoDepth(fifo_depth))throw std::invalid_argument("Invalid console FIFO depth");
}
void ConsoleDevice::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto port=devicePort(*this,b,l);
    auto fifo=devicePart(*this,b,circuit::families::synchronousFifo(8),"TX_FIFO",(fifo_depth_==4?circuit::ParameterMap{}:circuit::ParameterMap{{"depth",std::to_string(fifo_depth_)}}));
    inputs(l,*this,fifo,{"CLK","RST"});l.wire(getInputPin("TX_READY"),fifo->getInputPin("POP"));
    auto registers=circuit::device::buildRegisterFrontEnd(b,circuit::device::deviceRegisterTable("device.console",{}));
    frontEnd(l,port,registers);
    l.wire(fifo->getOutputPin("PUSH_READY"),registers->getInputPin("READY"));
    l.wire(fifo->getOutputPin("PUSH_READY"),registers->getInputPin("STATUS_VALUE"));
    l.wire(fifo->getOutputPin<3>("COUNT"),registers->getInputPin<3>("COUNT_VALUE"));
    l.wire(registers->getOutputPin("TX_WRITE"),fifo->getInputPin("PUSH"));
    auto byte=group(b,"TX_BYTE","ConsoleTxByte",{{"WRITE_DATA",32}},{{"BYTE",8}},
        [](IOComponent& self,ComponentBuilder& gates) {
            circuit::structural::Logic c(gates);auto word=c.split(input(self,"WRITE_DATA"));c.drive(V(word.begin(),word.begin()+8),output(self,"BYTE"));c.finish();
        });
    l.wire(port->getOutputPin<32>("WRITE_DATA"),byte->getInputPin<32>("WRITE_DATA"));l.wire(byte->getOutputPin<8>("BYTE"),fifo->getInputPin<8>("PUSH_DATA"));
    l.wire(l.bit(false),fifo->getInputPin("CLEAR_OVERFLOW"));
    l.wire(fifo->getOutputPin("VALID"),getOutputPin("TX_VALID"));l.wire(fifo->getOutputPin<8>("DATA"),getOutputPin<8>("TX_DATA"));l.finish();
}
TimerDevice::TimerDevice(std::string n,uint32_t divisor,uint32_t frequency):IOComponent(std::move(n),circuit::families::TimerDevice.pinInitializer()),divisor_(divisor),frequency_(frequency){
    if(!divisor)throw std::invalid_argument("Timer divisor must be positive");
}
void TimerDevice::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto port=devicePort(*this,b,l);
    auto counter=devicePart(*this,b,circuit::families::clockedCounter(32),"COUNTER",{{"divisor",std::to_string(divisor_)}});
    inputs(l,*this,counter,{"CLK","RST"});l.wire(l.bit(true),counter->getInputPin("ENABLE"));
    auto registers=circuit::device::buildRegisterFrontEnd(b,circuit::device::deviceRegisterTable("device.timer",
        {{"divisor",std::to_string(divisor_)},{"frequency_hz",std::to_string(frequency_)}}));
    frontEnd(l,port,registers);l.wire(l.bit(true),registers->getInputPin("READY"));
    l.wire(counter->getOutputPin<32>("COUNT"),registers->getInputPin<32>("COUNT_VALUE"));l.finish();
}
ButtonDevice::ButtonDevice(std::string n,size_t fifo_depth):IOComponent(std::move(n),buttonPins),fifo_depth_(fifo_depth){
    if(!circuit::device::validFifoDepth(fifo_depth))throw std::invalid_argument("Invalid button FIFO depth");
}
void ButtonDevice::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto port=devicePort(*this,b,l);
    auto fifo=devicePart(*this,b,circuit::families::synchronousFifo(32),"EVENTS",(fifo_depth_==4?circuit::ParameterMap{}:circuit::ParameterMap{{"depth",std::to_string(fifo_depth_)}}));
    inputs(l,*this,fifo,{"CLK","RST"});l.wire(getInputPin("INPUT_VALID"),fifo->getInputPin("PUSH"));
    auto levels=getSelectedFidelity()=="behavioral"
        ? devicePart(*this,b,circuit::families::registerWord(32),"LEVELS")
        : l.reg("LEVELS",32,getInputPin("CLK"),getInputPin("RST"),getInputPin("INPUT_VALID"));
    if(getSelectedFidelity()=="behavioral"){
        inputs(l,*this,levels,{"CLK","RST"});l.wire(getInputPin("INPUT_VALID"),levels->getInputPin("WE"));
    }
    // One Decoder<5> picks the reported button; its level becomes DOWN,
    // every other level is kept: NEXT = (LEVELS & ~SELECT) | (SELECT & DOWN).
    auto update=group(b,"INPUT_UPDATE","ButtonInputUpdate",{{"LEVELS",32},{"BUTTON",5},{"DOWN",1}},{{"NEXT",32}},
        [](IOComponent& self,ComponentBuilder& gates) {
            circuit::structural::Logic u(gates);
            auto decode=gates.add(circuit::families::decoder(5),"BUTTON_DECODE");
            u.wire(input(self,"BUTTON"),decode->getInputPinDynamic("ADDR"));u.wire(u.bit(true),decode->getInputPin("ENABLE"));
            V select;for(size_t i=0;i<32;++i)select.push_back(decode->getOutputPin("OUT"+std::to_string(i)));
            const auto gate=[&](circuit::word::GateKind kind,const std::string& name){return gates.add(circuit::families::wordGate(kind,32),name);};
            auto selected=u.join(select);
            auto unselected=gate(circuit::word::GateKind::Not,"UNSELECTED");u.wire(selected,unselected->getInputPinDynamic("IN"));
            auto keep=gate(circuit::word::GateKind::And,"KEEP");u.wire(input(self,"LEVELS"),keep->getInputPinDynamic("A"));
            u.wire(unselected->getOutputPinDynamic("OUT"),keep->getInputPinDynamic("B"));
            auto set=u.join(u.mask(select,input(self,"DOWN")));
            auto next=gate(circuit::word::GateKind::Or,"NEXT_LEVELS");u.wire(keep->getOutputPinDynamic("OUT"),next->getInputPinDynamic("A"));
            u.wire(set,next->getInputPinDynamic("B"));u.wire(next->getOutputPinDynamic("OUT"),output(self,"NEXT"));u.finish();
        });
    l.wire(levels->getOutputPin<32>("Q"),update->getInputPin<32>("LEVELS"));l.wire(getInputPin<5>("INPUT_BUTTON"),update->getInputPin<5>("BUTTON"));
    l.wire(getInputPin("INPUT_DOWN"),update->getInputPin("DOWN"));l.wire(update->getOutputPin<32>("NEXT"),levels->getInputPin<32>("D"));
    auto encoding=group(b,"EVENT_ENCODING","ButtonEventEncoding",{{"BUTTON",5},{"DOWN",1}},{{"EVENT",32}},
        [](IOComponent& self,ComponentBuilder& gates) {
            Logic e(gates);auto event=pad(e,e.split(input(self,"BUTTON")));event[8]=input(self,"DOWN");e.drive(event,output(self,"EVENT"));e.finish();
        });
    l.wire(getInputPin<5>("INPUT_BUTTON"),encoding->getInputPin<5>("BUTTON"));l.wire(getInputPin("INPUT_DOWN"),encoding->getInputPin("DOWN"));
    l.wire(encoding->getOutputPin<32>("EVENT"),fifo->getInputPin<32>("PUSH_DATA"));
    auto registers=circuit::device::buildRegisterFrontEnd(b,circuit::device::deviceRegisterTable("device.buttons",{}));
    frontEnd(l,port,registers);l.wire(l.bit(true),registers->getInputPin("READY"));
    l.wire(levels->getOutputPin<32>("Q"),registers->getInputPin<32>("LEVELS_VALUE"));
    l.wire(fifo->getOutputPin<32>("DATA"),registers->getInputPin<32>("HEAD_VALUE"));
    l.wire(fifo->getOutputPin<3>("COUNT"),registers->getInputPin<3>("COUNT_VALUE"));
    auto status=group(b,"STATUS_BITS","ButtonStatusBits",{{"VALID",1},{"OVERFLOW",1}},{{"STATUS",2}},
        [](IOComponent& self,ComponentBuilder& gates) {
            circuit::structural::Logic c(gates);c.drive({input(self,"VALID"),input(self,"OVERFLOW")},output(self,"STATUS"));c.finish();
        });
    for(auto name:{"VALID","OVERFLOW"})l.wire(fifo->getOutputPin(name),status->getInputPin(name));
    l.wire(status->getOutputPin<2>("STATUS"),registers->getInputPin<2>("STATUS_VALUE"));
    l.wire(registers->getOutputPin("POP_WRITE"),fifo->getInputPin("POP"));
    l.wire(registers->getOutputPin("CLEAR_OVERFLOW_WRITE"),fifo->getInputPin("CLEAR_OVERFLOW"));l.finish();
}
namespace circuit::device {
void registerComponents(ComponentCatalog&c) {
    for(size_t w:{4u,8u,16u,32u})circuit::registration::registerStructural(c,families::clockedCounter(w),"ClockedCounterTest","clock-boundary",detail::supportsCounter);
    for(size_t w:{8u,16u,32u})circuit::registration::registerStructural(c,families::synchronousFifo(w),"SynchronousFifoTest","clock-boundary",detail::supportsFifo);
    for(size_t w:{4u,32u})circuit::registration::registerBehavioral(c,families::clockedCounter(w),"ClockedCounterTest","ClockedCounterScenarioTest",detail::supportsCounter);
    for(size_t w:{8u,32u})circuit::registration::registerBehavioral(c,families::synchronousFifo(w),"SynchronousFifoTest","SynchronousFifoScenarioTest",detail::supportsFifo);
    circuit::registration::registerStructural(c,families::ConsoleDevice,"ConsoleDeviceTest","clock-boundary",detail::supportsQueuedDevice);circuit::registration::registerStructural(c,families::TimerDevice,"TimerDeviceTest","clock-boundary",detail::supportsTimer);circuit::registration::registerStructural(c,families::ButtonDevice,"ButtonDeviceTest","clock-boundary",detail::supportsQueuedDevice);
    circuit::registration::registerStructural(c,families::FramebufferDisplay,"FramebufferDisplayTest","clock-boundary",detail::supportsDisplay);
    circuit::registration::registerBehavioral(c,families::ConsoleDevice,"ConsoleDeviceTest","ConsoleDeviceTest",detail::supportsQueuedDevice,"clock-boundary");
    circuit::registration::registerBehavioral(c,families::TimerDevice,"TimerDeviceTest","TimerDeviceTest",detail::supportsTimer,"clock-boundary");
    circuit::registration::registerBehavioral(c,families::ButtonDevice,"ButtonDeviceTest","ButtonDeviceTest",detail::supportsQueuedDevice,"clock-boundary");
    circuit::registration::registerBehavioral(c,families::FramebufferDisplay,"FramebufferDisplayTest","FramebufferDisplayTest",detail::supportsDisplay,"clock-boundary");
    circuit::registration::registerStructural(c,families::LedPanelDevice,"LedPanelTest","clock-boundary");
    circuit::registration::registerBehavioral(c,families::LedPanelDevice,"LedPanelTest","LedPanelTest",{},"clock-boundary");
}
}
