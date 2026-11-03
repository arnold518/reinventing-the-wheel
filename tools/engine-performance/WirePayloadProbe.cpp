#include "basic/Wire.hpp"
#include "simulator/Simulator.hpp"
#include "simulator/SignalTrace.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using L=LogicValue;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
class Observer final:public SignalTraceObserver {
public:
    size_t count=0;uint64_t digest=1469598103934665603ULL;
    void wireChanged(SimulationTime time,const std::shared_ptr<WireBase>&,
                     const std::vector<L>& values)override{
        ++count;digest=(digest^time)*1099511628211ULL;
        for(auto bit:values)digest=(digest^unsigned(bit))*1099511628211ULL;
    }
    void pinChanged(SimulationTime,const std::shared_ptr<PinBase>&,const std::vector<L>&)override{}
    void settledThrough(SimulationTime)override{}
};

// Its virtual value interface deliberately stores state outside the base array.
// A typed fast path must retain the original behavior for derived wires.
class ProbeDerivedWire final:public Wire<1> {
    L stored_=L::HIGH;
public:
    ProbeDerivedWire():Wire<1>("DERIVED"){}
    L getBit(size_t index)const override{if(index)throw std::out_of_range("bit");return stored_;}
    std::vector<L> getValueVector()const override{return {stored_};}
    void setValueVector(const std::vector<L>& value)override{stored_=value.empty()?L::UNKNOWN:value[0];}
    L getSingleValue()const override{return stored_;}
};

template<size_t W>
void plain(bool history) {
    Simulator sim;sim.setHistoryRecordingEnabled(history);
    const auto observer=std::make_shared<Observer>();SignalTraceSubscription trace(sim,observer);
    const auto wire=std::make_shared<Wire<W>>("PLAIN");
    std::vector<L> previous(W,L::UNKNOWN);size_t changes=0;
    for(unsigned pattern=0;pattern<128;++pattern)for(unsigned repeat=0;repeat<2;++repeat) {
        const SimulationTime time=(SimulationTime{1}<<63)+pattern*3+repeat;
        std::vector<L> payload(W);
        for(size_t bit=0;bit<W;++bit)payload[bit]=L((pattern+bit+(pattern>>2))%4);
        // The factory pads short payloads with UNKNOWN and truncates long ones.
        if(pattern%7==0)payload.resize(W/2);
        if(pattern%11==0)payload.resize(W+2,L::HIGH_Z);
        std::vector<L> expected(W,L::UNKNOWN);
        for(size_t bit=0;bit<W&&bit<payload.size();++bit)expected[bit]=payload[bit];
        if(expected!=previous)++changes;
        sim.scheduleEvent(makeWireUpdateEvent(time,wire,payload));
        const auto drain=sim.drainUntilIdle(time,1);
        require(drain.processed_events==1,"Wire quantum differs");
        require(wire->getValueVector()==expected,"Wire value differs");
        require(sim.getPerformanceCounters().effective_wire_changes==changes,"No-op/change counters differ");
        require(observer->count==changes,"Wire observer includes a no-op or missed a change");
        previous=expected;
    }
    sim.scheduleEvent(std::make_shared<WireUpdateEvent>(MAX_SIMULATION_TIME,std::shared_ptr<Wire<W>>{},L::LOW));
    sim.drainUntilIdle(MAX_SIMULATION_TIME,1);
    require(observer->count==changes,"Null wire published a change");
    std::cout<<W<<' '<<history<<' '<<changes<<' '<<observer->digest<<'\n';
}
void derived() {
    Simulator sim;sim.setHistoryRecordingEnabled(false);
    auto wire=std::make_shared<ProbeDerivedWire>();
    sim.scheduleEvent(makeWireUpdateEvent(1,wire,{L::HIGH}));sim.drainUntilIdle(1,1);
    require(sim.getPerformanceCounters().effective_wire_changes==0,"Derived wire bypassed virtual no-op comparison");
    sim.scheduleEvent(makeWireUpdateEvent(2,wire,{L::LOW}));sim.drainUntilIdle(2,1);
    require(sim.getPerformanceCounters().effective_wire_changes==1&&wire->getBit(0)==L::LOW,
            "Derived wire bypassed virtual assignment");
}
}
int main(){
    try{
        for(bool history:{false,true}){
            plain<1>(history);plain<2>(history);plain<3>(history);plain<4>(history);
            plain<5>(history);plain<8>(history);plain<16>(history);plain<32>(history);
        }
        derived();
        std::cout<<"PASS every supported width/four-state payload, duplicate changes, short/long inputs, uint64 time, null/derived wires and observation/history modes\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
