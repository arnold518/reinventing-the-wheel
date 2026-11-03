#include "TickTraceWriter.hpp"
#include "components/IOComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <iostream>
#include <set>

class TraceMemory : public IOComponent, public MemoryStateView {
public:
    explicit TraceMemory(std::string name):IOComponent(std::move(name)),storage(4,8,LogicValue::LOW){}
    const MemoryStorage& memoryStorage()const override{return storage;}
    void loadMemoryBytes(uint32_t address,const std::vector<uint8_t>& bytes)override{storage.loadBytes(address,bytes);}
    MemoryStorage storage;
};
class TraceRegisters : public IOComponent,public RegisterStateView {
public:
    explicit TraceRegisters(std::string name):IOComponent(std::move(name)){}
    std::vector<std::vector<LogicValue>> getRegisterStateAtTime(SimulationTime time)const override{
        auto values=std::vector<std::vector<LogicValue>>(2,std::vector<LogicValue>(32,LogicValue::LOW));
        if(time>=change_time)values[1][3]=LogicValue::HIGH;return values;
    }
    SimulationTime change_time=MAX_SIMULATION_TIME;
};

std::string bits(const std::vector<LogicValue>& values) {
    std::string result;for(auto it=values.rbegin();it!=values.rend();++it)result+="01XZ"[unsigned(*it)];return result;
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::invalid_argument("Usage: SignalTraceProbe new-directory");
        const std::filesystem::path directory=argv[1];
        if(std::filesystem::exists(directory))throw std::invalid_argument("Output already exists");
        std::filesystem::create_directories(directory);
        auto root=Component::create<IOComponent>("TRACE");
        auto a=root->addPinDynamic("A",PinType::INPUT,8),b=root->addPinDynamic("B",PinType::INPUT,32);
        auto out=root->addPinDynamic("OUT",PinType::OUTPUT,1);
        ComponentBuilder builder(root);
        auto wa=builder.addNewWireDynamic("A",8,nullptr,{a});
        auto wb=builder.addNewWireDynamic("B",32,nullptr,{b});
        auto memory=builder.addNewComponent<TraceMemory>("MEMORY");
        auto registers=builder.addNewComponent<TraceRegisters>("REGISTERS");
        Simulator sim;
        circuit::visualizer::VisualizerSession view(root,sim,"trace-equipment");
        auto writer=std::make_shared<TickTraceWriter>(directory,view,1024);
        const SimulationTime base=(SimulationTime{1}<<63)+123;
        {
            SignalTraceSubscription subscription(sim,writer);
            // A pin-only publication is observed before its future time. Two
            // wire events share a timestamp and a quantum ends between them.
            sim.recordPinChange(base+13,out,{LogicValue::HIGH_Z});
            sim.recordPinChange(base+19,out,{LogicValue::HIGH});
            sim.recordPinChange(base+29,a,std::vector<LogicValue>(8,LogicValue::HIGH_Z));
            memory->storage.writeLogic(base+15,1,{LogicValue::HIGH,LogicValue::LOW,LogicValue::HIGH_Z,LogicValue::UNKNOWN,
                LogicValue::HIGH,LogicValue::LOW,LogicValue::HIGH,LogicValue::LOW},true);
            memory->storage.fill(base+23,LogicValue::LOW);
            registers->change_time=base+21;
            writer->registersChanged(base+21,registers,registers->getRegisterStateAtTime(base+21));
            for(size_t i=0;i<1200;++i){
                std::vector<LogicValue> av(8),bv(32);
                for(size_t j=0;j<av.size();++j)av[j]=LogicValue((i+j)%4);
                for(size_t j=0;j<bv.size();++j)bv[j]=LogicValue((i/3+j)%4);
                sim.scheduleEvent(makeWireUpdateEvent(base+3+i*2,wa,av));
                sim.scheduleEvent(makeWireUpdateEvent(base+3+i*2,wb,bv));
            }
            auto cancelled=makeWireUpdateEvent(base+8,wa,std::vector<LogicValue>(8,LogicValue::LOW));
            cancelled->cancelled=true;sim.scheduleEvent(cancelled);
            while(sim.drainUntilIdle(base+2500,1).status==DrainStatus::EventLimitReached){}
            writer->presentation(base+2500);
        }
        if(SignalTraceSubscription::observer(sim))throw std::logic_error("Trace subscription leaked");
        writer->finish();
        const auto before=sim.getPerformanceCounters();
        std::set<SimulationTime> times;for(auto t:sim.getUniqueTimestamps())times.insert(t);times.insert(base+2500);
        for(auto offset:{15,21,23})times.insert(base+offset);
        std::ofstream expected(directory/"expected.json");
        expected<<"{\"pinWidths\":[";view.forEachPin([&](size_t i,const auto& p){if(i)expected<<',';expected<<p->getWidth();});
        expected<<"],\"wireWidths\":[";view.forEachWire([&](size_t i,const auto& w){if(i)expected<<',';expected<<w->getWidth();});
        expected<<"],\"states\":[";
        bool first=true;
        for(auto time:times){
            if(!first)expected<<',';first=false;
            sim.setCircuitStateAtTime(time);
            expected<<"{\"time\":\""<<time<<"\",\"pins\":[";
            view.forEachPin([&](size_t i,const auto& pin){
                const auto& history=sim.signalHistory(pin);
                auto end=std::upper_bound(history.begin(),history.end(),time,[](SimulationTime t,const auto& e){return t<e.first;});
                if(i)expected<<',';expected<<'"'<<bits(end==history.begin()?std::vector<LogicValue>(pin->getWidth(),LogicValue::UNKNOWN):std::prev(end)->second)<<'"';
            });
            expected<<"],\"wires\":[";view.forEachWire([&](size_t i,const auto& wire){if(i)expected<<',';expected<<'"'<<bits(wire->getValueVector())<<'"';});
            expected<<"],\"auxiliary\":[";bool first_aux=true;
            for(const auto& word:memory->storage.getWordsAtTime(time,0,4)){if(!first_aux)expected<<',';first_aux=false;expected<<'"'<<bits(word.second)<<'"';}
            for(const auto& word:registers->getRegisterStateAtTime(time))expected<<",\""<<bits(word)<<'"';expected<<"]}";
        }
        expected<<"]}\n";expected.close();
        if(!expected||writer->timestampCount()!=times.size()||before.processed_events!=sim.getPerformanceCounters().processed_events)
            throw std::logic_error("Trace differs from retained history");
        std::cout<<"PASS "<<times.size()<<" full ticks, four-state buses, future pin-only changes, single-event quanta and cancellation\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
