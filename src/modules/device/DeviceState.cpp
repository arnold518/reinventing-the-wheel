#include "components/ComponentBuilder.tpp"
#include "DeviceLogic.hpp"
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
const ComponentFamily ConsoleDevice{"device.console","ConsoleDevice",consolePins,[](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::ConsoleDevice>(c,n,parameter(c,"fifo_depth",4));}};
const ComponentFamily TimerDevice{"device.timer","TimerDevice",[](IOComponent*p){memory::protocolPins(p,32);},
    [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::TimerDevice>(c,n,
        circuit::device::detail::parameter(c,"divisor",1),circuit::device::detail::parameter(c,"frequency_hz",0));}};
const ComponentFamily ButtonDevice{"device.buttons","ButtonDevice",buttonPins,[](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::ButtonDevice>(c,n,parameter(c,"fifo_depth",4));}};
}
ClockedCounter::ClockedCounter(std::string n,size_t w,uint32_t divisor):IOComponent(std::move(n),[w](IOComponent*p){counterPins(p,w);}),
    width_(w),divisor_(divisor),type_("ClockedCounter<"+std::to_string(w)+">"){if(!divisor)throw std::invalid_argument("Counter divisor must be positive");}
void ClockedCounter::buildInternals(ComponentBuilder&b){
    Logic l(b);auto enable=l.land({getInputPin("ENABLE"),l.inv(getInputPin("RST"))});auto tick=enable;
    if(divisor_!=1){
        auto div=l.reg("DIVIDER",32,getInputPin("CLK"),getInputPin("RST"),getInputPin("ENABLE"));
        auto v=l.split(div->getOutputPin<32>("Q"));auto last=l.equal({v,l.constant(32,divisor_-1)});
        auto next=l.add({v,l.constant(32,1),l.bit(false)}).first;
        l.drive(l.mux({last,next,l.constant(32)}),div->getInputPin<32>("D"));tick=l.land({enable,last});
    }
    auto count=l.reg("COUNT_STATE",width_,getInputPin("CLK"),getInputPin("RST"),tick);
    l.drive(l.add({l.split(count->getOutputPinDynamic("Q")),l.constant(width_,1),l.bit(false)}).first,count->getInputPinDynamic("D"));
    l.out(count->getOutputPinDynamic("Q"),getOutputPinDynamic("COUNT"));l.out(tick,getOutputPin("TICK"));l.finish();
}
SynchronousFifo::SynchronousFifo(std::string n,size_t w,size_t depth):IOComponent(std::move(n),[w](IOComponent*p){fifoPins(p,w);}),
    width_(w),depth_(depth),type_("SynchronousFifo<"+std::to_string(w)+">"){
    if(!circuit::device::validFifoDepth(depth))throw std::invalid_argument("FIFO depth must be in 1..7 (three-bit COUNT)");
}
void SynchronousFifo::buildInternals(ComponentBuilder&b){
    Logic l(b);auto clk=getInputPin("CLK"),rst=getInputPin("RST");
    auto count=l.reg("COUNT_STATE",3,clk,rst,l.bit(true));auto c=l.split(count->getOutputPin<3>("Q"));
    auto empty=l.equal({c,l.constant(3)}),full=l.equal({c,l.constant(3,depth_)});
    auto ready=l.land({l.inv(rst),l.inv(full)}),valid=l.land({l.inv(rst),l.inv(empty)});
    auto push=l.land({getInputPin("PUSH"),ready}),pop=l.land({getInputPin("POP"),valid});
    auto inc=l.add({c,l.constant(3),push}).first;
    l.drive(l.sub({inc,{pop,l.bit(false),l.bit(false)}}).first,count->getInputPin<3>("D"));
    const size_t pointer_width=depth_<=4?2:3;
    auto head=l.reg("HEAD",pointer_width,clk,rst,pop),tail=l.reg("TAIL",pointer_width,clk,rst,push);
    for(auto ptr:{head,tail}) {
        const auto value=l.split(ptr->getOutputPinDynamic("Q"));
        auto next=l.add({value,l.constant(pointer_width,1),l.bit(false)}).first;
        if(depth_!=4)next=l.mux({l.equal({value,l.constant(pointer_width,depth_-1)}),next,l.constant(pointer_width)});
        l.drive(next,ptr->getInputPinDynamic("D"));
    }
    V data=l.constant(width_);auto hv=l.split(head->getOutputPinDynamic("Q")),tv=l.split(tail->getOutputPinDynamic("Q"));
    for(size_t i=0;i<depth_;++i){
        auto slot=l.reg("SLOT_"+std::to_string(i),width_,clk,rst,l.land({push,l.equal({tv,l.constant(pointer_width,i)})}));
        l.wire(getInputPinDynamic("PUSH_DATA"),slot->getInputPinDynamic("D"));
        data=l.mux({l.equal({hv,l.constant(pointer_width,i)}),data,l.split(slot->getOutputPinDynamic("Q"))});
    }
    auto overflow=l.reg("OVERFLOW_STATE",1,clk,rst,l.bit(true));
    auto dropped=l.land({getInputPin("PUSH"),full});
    l.wire(l.lor({dropped,l.land({overflow->getOutputPin("Q"),l.inv(getInputPin("CLEAR_OVERFLOW"))})}),overflow->getInputPin("D"));
    l.out(overflow->getOutputPin("Q"),getOutputPin("OVERFLOW"));l.out(ready,getOutputPin("PUSH_READY"));
    l.out(valid,getOutputPin("VALID"));l.out(full,getOutputPin("FULL"));l.out(count->getOutputPin<3>("Q"),getOutputPin<3>("COUNT"));
    l.drive(l.mux({valid,l.constant(width_),data}),getOutputPinDynamic("DATA"));l.finish();
}
ConsoleDevice::ConsoleDevice(std::string n,size_t fifo_depth):IOComponent(std::move(n),consolePins),fifo_depth_(fifo_depth){
    if(!circuit::device::validFifoDepth(fifo_depth))throw std::invalid_argument("Invalid console FIFO depth");
}
void ConsoleDevice::buildInternals(ComponentBuilder&b){
    Logic l(b);auto port=devicePort(*this,b,l);auto fifo=b.add(circuit::families::synchronousFifo(8),"TX_FIFO",(fifo_depth_==4?circuit::ParameterMap{}:circuit::ParameterMap{{"depth",std::to_string(fifo_depth_)}}));
    for(auto p:{"CLK","RST"})l.wire(getInputPin(p),fifo->getInputPin(p));
    auto tx=at(l,port,0),status=at(l,port,4),count=at(l,port,8);
    auto wr=port->getOutputPin("IS_WRITE"),rd=port->getOutputPin("IS_READ");
    auto word=l.split(port->getOutputPin<32>("WRITE_DATA")),mask=l.split(port->getOutputPin<4>("WRITE_MASK"));
    auto payload_ok=l.land({knownWord(l,b,port->getOutputPin<32>("WRITE_DATA")),l.inv(l.any(V(word.begin()+8,word.end())))});
    auto result=l.mux({l.lor({status,count}),l.constant(3,2),l.mux({wr,l.constant(3),l.constant(3,3)})});
    result=l.mux({tx,result,l.mux({rd,l.constant(3),l.constant(3,4)})});
    result=l.mux({l.land({tx,l.land({wr,l.land({l.any(mask),l.inv(payload_ok)})})}),result,l.constant(3,6)});
    l.drive(result,port->getInputPin<3>("BACK_STATUS"));
    V data=l.constant(32);
    data=l.mux({status,data,pad(l,{fifo->getOutputPin("PUSH_READY")})});
    data=l.mux({count,data,pad(l,l.split(fifo->getOutputPin<3>("COUNT")))});
    l.drive(data,port->getInputPin<32>("BACK_READ_DATA"));l.wire(fifo->getOutputPin("PUSH_READY"),port->getInputPin("BACK_READY"));
    l.wire(l.land({tx,port->getOutputPin("WRITE_STROBE")}),fifo->getInputPin("PUSH"));
    l.drive(V(word.begin(),word.begin()+8),fifo->getInputPin<8>("PUSH_DATA"));
    l.wire(getInputPin("TX_READY"),fifo->getInputPin("POP"));l.wire(l.bit(false),fifo->getInputPin("CLEAR_OVERFLOW"));
    l.out(fifo->getOutputPin("VALID"),getOutputPin("TX_VALID"));l.out(fifo->getOutputPin<8>("DATA"),getOutputPin<8>("TX_DATA"));l.finish();
}
TimerDevice::TimerDevice(std::string n,uint32_t divisor,uint32_t frequency):IOComponent(std::move(n),circuit::families::TimerDevice.pinInitializer()),divisor_(divisor),frequency_(frequency){
    if(!divisor)throw std::invalid_argument("Timer divisor must be positive");
}
void TimerDevice::buildInternals(ComponentBuilder&b){
    Logic l(b);auto port=devicePort(*this,b,l);auto counter=b.add(circuit::families::clockedCounter(32),"COUNTER",{{"divisor",std::to_string(divisor_)}});
    for(auto p:{"CLK","RST"})l.wire(getInputPin(p),counter->getInputPin(p));l.wire(l.bit(true),counter->getInputPin("ENABLE"));
    auto ticks=at(l,port,0),div=at(l,port,4),hz=at(l,port,8);
    auto mapped=l.any({ticks,div,hz});
    auto status=l.mux({mapped,l.constant(3,2),l.mux({port->getOutputPin("IS_WRITE"),l.constant(3),l.constant(3,3)})});
    l.drive(status,port->getInputPin<3>("BACK_STATUS"));l.wire(l.bit(true),port->getInputPin("BACK_READY"));
    V data=l.constant(32);data=l.mux({ticks,data,l.split(counter->getOutputPin<32>("COUNT"))});
    data=l.mux({div,data,l.constant(32,divisor_)});data=l.mux({hz,data,l.constant(32,frequency_)});
    l.drive(data,port->getInputPin<32>("BACK_READ_DATA"));l.finish();
}
ButtonDevice::ButtonDevice(std::string n,size_t fifo_depth):IOComponent(std::move(n),buttonPins),fifo_depth_(fifo_depth){
    if(!circuit::device::validFifoDepth(fifo_depth))throw std::invalid_argument("Invalid button FIFO depth");
}
void ButtonDevice::buildInternals(ComponentBuilder&b){
    Logic l(b);auto port=devicePort(*this,b,l);auto fifo=b.add(circuit::families::synchronousFifo(32),"EVENTS",(fifo_depth_==4?circuit::ParameterMap{}:circuit::ParameterMap{{"depth",std::to_string(fifo_depth_)}}));
    for(auto p:{"CLK","RST"})l.wire(getInputPin(p),fifo->getInputPin(p));
    auto id=l.split(getInputPin<5>("INPUT_BUTTON"));auto input=getInputPin("INPUT_VALID");
    auto levels=l.reg("LEVELS",32,getInputPin("CLK"),getInputPin("RST"),input);auto old=l.split(levels->getOutputPin<32>("Q"));V next;
    for(size_t i=0;i<32;++i)next.push_back(l.choose({l.equal({id,l.constant(5,i)}),old[i],getInputPin("INPUT_DOWN")}));
    l.drive(next,levels->getInputPin<32>("D"));
    auto event=pad(l,id);event[8]=getInputPin("INPUT_DOWN");l.drive(event,fifo->getInputPin<32>("PUSH_DATA"));l.wire(input,fifo->getInputPin("PUSH"));
    auto lev=at(l,port,0),head=at(l,port,4),count=at(l,port,8),status=at(l,port,12),pop=at(l,port,16),clear=at(l,port,20);
    auto writable=l.lor({pop,clear}),readable=l.any({lev,head,count,status});
    auto mapped=l.lor({writable,readable});auto wr=port->getOutputPin("IS_WRITE");
    auto result=l.mux({mapped,l.constant(3,2),l.mux({wr,l.constant(3),l.constant(3,3)})});
    result=l.mux({writable,result,l.mux({wr,l.constant(3,4),l.constant(3)})});
    auto mask=l.any(l.split(port->getOutputPin<4>("WRITE_MASK")));
    result=l.mux({l.land({writable,l.land({wr,l.land({mask,l.inv(validCommand(l,b,port))})})}),result,l.constant(3,6)});
    l.drive(result,port->getInputPin<3>("BACK_STATUS"));l.wire(l.bit(true),port->getInputPin("BACK_READY"));
    V data=l.constant(32);data=l.mux({lev,data,l.split(levels->getOutputPin<32>("Q"))});
    data=l.mux({head,data,l.split(fifo->getOutputPin<32>("DATA"))});data=l.mux({count,data,pad(l,l.split(fifo->getOutputPin<3>("COUNT")))});
    data=l.mux({status,data,pad(l,{fifo->getOutputPin("VALID"),fifo->getOutputPin("OVERFLOW")})});l.drive(data,port->getInputPin<32>("BACK_READ_DATA"));
    auto bit=l.split(port->getOutputPin<32>("WRITE_DATA"))[0];auto command=l.land({port->getOutputPin("WRITE_STROBE"),bit});
    l.wire(l.land({command,pop}),fifo->getInputPin("POP"));l.wire(l.land({command,clear}),fifo->getInputPin("CLEAR_OVERFLOW"));l.finish();
}
namespace circuit::device {
void registerComponents(ComponentCatalog&c) {
    for(size_t w:{4u,8u,16u,32u})circuit::registration::registerStructural(c,families::clockedCounter(w),"ClockedCounterTest","clock-boundary",detail::supportsCounter);
    for(size_t w:{8u,16u,32u})circuit::registration::registerStructural(c,families::synchronousFifo(w),"SynchronousFifoTest","clock-boundary",detail::supportsFifo);
    for(size_t w:{4u,32u})circuit::registration::registerBehavioral(c,families::clockedCounter(w),"ClockedCounterTest","ClockedCounterScenarioTest",detail::supportsCounter);
    for(size_t w:{8u,32u})circuit::registration::registerBehavioral(c,families::synchronousFifo(w),"SynchronousFifoTest","SynchronousFifoScenarioTest",detail::supportsFifo);
    circuit::registration::registerStructural(c,families::ConsoleDevice,"ConsoleDeviceTest","clock-boundary",detail::supportsQueuedDevice);circuit::registration::registerStructural(c,families::TimerDevice,"TimerDeviceTest","clock-boundary",detail::supportsTimer);circuit::registration::registerStructural(c,families::ButtonDevice,"ButtonDeviceTest","clock-boundary",detail::supportsQueuedDevice);
    circuit::registration::registerStructural(c,families::FramebufferDisplay,"FramebufferDisplayTest","clock-boundary",detail::supportsDisplay);
}
}
