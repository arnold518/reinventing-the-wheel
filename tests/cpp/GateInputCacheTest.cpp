#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/TimingProfile.hpp"
#include "modules/basic/Gate.hpp"
#include "simulator/SignalTrace.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
using L=LogicValue;
constexpr L low=L::LOW,high=L::HIGH,x=L::UNKNOWN,z=L::HIGH_Z;
using Table=std::array<std::array<L,4>,4>;
constexpr Table and_table{{{{low,low,low,low}},{{low,high,x,x}},{{low,x,x,x}},{{low,x,x,x}}}};
constexpr Table or_table{{{{low,high,x,x}},{{high,high,high,high}},{{x,high,x,x}},{{x,high,x,x}}}};
constexpr Table xor_table{{{{low,high,x,x}},{{high,low,x,x}},{{x,x,x,x}},{{x,x,x,x}}}};
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
L inverted(L value){return value==low?high:value==high?low:x;}

template<class Gate>
void binary(const Table& table,bool invert) {
    auto gate=Component::create<Gate>("GATE");ComponentBuilder b(gate);
    auto a=b.addNewWire("A",nullptr,{gate->template getInputPin<1>("A")});
    auto bb=b.addNewWire("B",nullptr,{gate->template getInputPin<1>("B")});
    auto out=b.addNewWire("OUT",gate->template getOutputPin<1>("OUT"));
    Simulator sim;SimulationTime time=1000;
    const auto delay=gate->getDelay(),spacing=delay+2;
    auto expected=[&](L aa,L bv){const auto value=table[unsigned(aa)][unsigned(bv)];return invert?inverted(value):value;};
    auto settle=[&](L value) {
        const auto old=out->getSingleValue();
        sim.drainUntilIdle(time+delay-1,1000);
        require(out->getSingleValue()==old,"Gate output moved before its delay");
        sim.drainUntilIdle(time+delay,1000);
        require(out->getSingleValue()==value,"Gate value differs after settling");
        if(value!=old)require(sim.signalHistory(gate->template getOutputPin<1>("OUT")).back().first==time+delay,
                              "Gate history has wrong transition time");
        time+=spacing;
    };
    for(L av:{low,high,x,z})for(L bv:{low,high,x,z}) {
        sim.scheduleEvent(std::make_shared<WireUpdateEvent>(time,a,av));
        sim.scheduleEvent(std::make_shared<WireUpdateEvent>(time,bb,bv));
        settle(expected(av,bv));
    }
    auto old=gate->template getInputPin<1>("A");std::weak_ptr<Pin<1>> old_weak=old;
    auto replacement=gate->template addPin<1>("A",PinType::INPUT);
    old.reset();require(old_weak.expired(),"Input cache retained a pin after replacement");
    replacement->setValueFromVector({high});gate->template getInputPin<1>("B")->setValueFromVector({low});
    gate->evaluateAt(time,sim);settle(expected(high,low));
    gate->template addPin<8>("A",PinType::INPUT)->setValueFromUInt64(255);
    for(L bv:{low,high}) {
        gate->template getInputPin<1>("B")->setValueFromVector({bv});
        gate->evaluateAt(time,sim);settle(expected(x,bv));
    }
    replacement=gate->template addPin<1>("A",PinType::INPUT);replacement->setValueFromVector({low});
    gate->template getInputPin<1>("B")->setValueFromVector({low});
    gate->template addPin<8>("EXTRA",PinType::INPUT);
    gate->evaluateAt(time,sim);settle(expected(low,low));
    auto rewired=b.addNewWire("NEW_A",nullptr,{replacement});
    sim.scheduleEvent(std::make_shared<WireUpdateEvent>(time,rewired,high));settle(expected(high,low));
    auto new_output=gate->template addPin<1>("OUT",PinType::OUTPUT);
    out=b.addNewWire("NEW_OUT",new_output);
    gate->evaluateAt(time,sim);settle(expected(high,low));
}

void unary() {
    auto gate=Component::create<NOTGate>("NOT");ComponentBuilder b(gate);
    auto in=b.addNewWire("IN",nullptr,{gate->getInputPin<1>("IN")});
    auto out=b.addNewWire("OUT",gate->getOutputPin<1>("OUT"));
    Simulator sim;SimulationTime time=1000;
    for(L value:{low,high,x,z}) {
        sim.scheduleEvent(std::make_shared<WireUpdateEvent>(time,in,value));
        sim.drainUntilIdle(time+gate->getDelay(),1000);
        require(out->getSingleValue()==inverted(value),"NOT four-state input mismatch");time+=gate->getDelay()+2;
    }
    auto replacement=gate->addPin<1>("IN",PinType::INPUT);replacement->setValueFromVector({low});
    gate->evaluateAt(time,sim);sim.drainUntilIdle(time+gate->getDelay(),1000);
    require(out->getSingleValue()==high,"NOT did not refresh replaced input");time+=gate->getDelay()+2;
    gate->addPin<8>("IN",PinType::INPUT)->setValueFromUInt64(1);
    gate->evaluateAt(time,sim);sim.drainUntilIdle(time+gate->getDelay(),1000);
    require(out->getSingleValue()==x,"NOT wrong-width input no longer resolves as unknown");
}

class PinObserver final:public SignalTraceObserver {
public:
    std::vector<std::pair<SimulationTime,std::vector<L>>> observed;
    void wireChanged(SimulationTime,const std::shared_ptr<WireBase>&,const std::vector<L>&)override{}
    void pinChanged(SimulationTime time,const std::shared_ptr<PinBase>&,const std::vector<L>& value)override{observed.emplace_back(time,value);}
    void settledThrough(SimulationTime)override{}
};
void headless_epochs() {
    auto root=Component::create<IOComponent>("PINS");auto pin=root->addPin<8>("OUT",PinType::OUTPUT);
    Simulator sim;sim.setHistoryRecordingEnabled(false);
    auto observer=std::make_shared<PinObserver>();SignalTraceSubscription trace(sim,observer);
    const std::vector<L> first{low,high,x,z,low,high,x,z},second(8,high);
    sim.recordPinChange(100,pin,first);sim.recordPinChange(200,pin,first);sim.recordPinChange(40,pin,second);
    require(observer->observed==std::vector<std::pair<SimulationTime,std::vector<L>>>{{100,first},{40,second}},
            "Headless value deduplication or future observation changed");
    require(sim.getPerformanceCounters().effective_pin_changes==2&&sim.signalHistory(pin).empty(),"Headless history/counters differ");
    sim.setHistoryRecordingEnabled(true);sim.recordPinChange(300,pin,second);
    require(sim.signalHistory(pin).size()==1&&sim.signalHistory(pin).back().first==300,"Re-enabled history epoch differs");
    sim.setHistoryRecordingEnabled(false);sim.recordPinChange(400,pin,second);
    require(sim.getPerformanceCounters().effective_pin_changes==4&&observer->observed.size()==4,"New headless epoch reused stale deduplication state");
    sim.clear();sim.recordPinChange(500,pin,second);
    require(sim.getPerformanceCounters().effective_pin_changes==1,"Clear retained headless state");
}
}
int main() {
    try {
        for(const auto& profile:circuit::timing::timingProfileNames()) {
            circuit::timing::setActiveTimingProfile(profile);
            unary();binary<ANDGate>(and_table,false);binary<NANDGate>(and_table,true);
            binary<ORGate>(or_table,false);binary<NORGate>(or_table,true);binary<XORGate>(xor_table,false);
        }
        headless_epochs();
        std::cout<<"PASS cached gate four-state values, delay histories, replacement/width/rewiring, ownership and headless epochs in every timing profile\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
