// Ordering contract independent of either heap or timestamp-bucket storage.
#include "simulator/Simulator.hpp"
#include "simulator/SignalTrace.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>

class Action final:public Event {
    EventPriority priority;
    std::function<void(Simulator&)> body;
public:
    Action(SimulationTime t,EventPriority p,std::function<void(Simulator&)> f)
        :Event(t),priority(p),body(std::move(f)){}
    EventPriority getPriority()const override{return priority;}
    void process(Simulator& sim)override{body(sim);}
};
class Frontier final:public SignalTraceObserver {
public:
    std::vector<SimulationTime> values;
    void wireChanged(SimulationTime,const std::shared_ptr<WireBase>&,const std::vector<LogicValue>&)override{}
    void pinChanged(SimulationTime,const std::shared_ptr<PinBase>&,const std::vector<LogicValue>&)override{}
    void settledThrough(SimulationTime t)override{values.push_back(t);}
};
int main() {
    try {
        Simulator sim;sim.setHistoryRecordingEnabled(false);
        const SimulationTime t=(SimulationTime{1}<<63)+100;
        std::vector<int> order;
        auto frontier=std::make_shared<Frontier>();SignalTraceSubscription trace(sim,frontier);
        auto add=[&](SimulationTime at,EventPriority p,int value) {
            auto event=std::make_shared<Action>(at,p,[&,value](Simulator&){order.push_back(value);});
            sim.scheduleEvent(event);return event;
        };
        sim.scheduleEvent(std::make_shared<Action>(t,EventPriority::COMPONENT_EVAL,[&](Simulator&){
            order.push_back(1);add(t,EventPriority::WIRE_UPDATE,5);
        }));
        add(t,EventPriority::COMPONENT_EVAL,2);
        add(t,EventPriority::WIRE_UPDATE,0);
        add(t+1,EventPriority::WIRE_UPDATE,99)->cancelled=true;
        add(t+2,EventPriority::COMPONENT_EVAL,3);
        size_t calls=0;
        while(sim.hasPendingEvents()) {
            const auto result=sim.drainUntilIdle(t+2,1);
            if(result.processed_events!=1)throw std::runtime_error("Wrong event quantum");
            ++calls;
        }
        if(order!=std::vector<int>{0,1,5,2,3}||calls!=5)
            throw std::runtime_error("Priority, FIFO, cancellation or same-time wire preemption differs");
        if(frontier->values!=std::vector<SimulationTime>{t-1,t,t+1,t+2})
            throw std::runtime_error("Settled frontier changed at a single-event quantum boundary");
        add(t+10,EventPriority::WIRE_UPDATE,7);
        sim.clear();
        if(sim.hasPendingEvents()||sim.nextEventTime()||sim.getCurrentTime()!=0||sim.getPerformanceCounters().processed_events)
            throw std::runtime_error("Clear did not reset pending work/time/counters");
        std::cout<<"PASS priority/FIFO, zero-delay wire preemption, cancellation, uint64 time, quantum frontier and clear\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
