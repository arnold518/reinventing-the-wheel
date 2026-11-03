// Bounded native performance equipment. Never changes production fidelity or
// event semantics. Guest instructions are checked by RecordedGuest's oracle.
#include "benchmark/Benchmark.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "runtime/RecordedGuest.hpp"
#include "simulator/SignalTrace.hpp"
#include "simulator/SimulationTest.hpp"
#include "visualizer/VisualizerSession.hpp"
#include "TickTraceWriter.hpp"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using Clock = std::chrono::steady_clock;
using circuit::benchmark::RunRecord;

struct Digest : SignalTraceObserver {
    uint64_t wires=0, pins=0, hash=1469598103934665603ULL;
    void mix(uint64_t value) { hash=(hash^value)*1099511628211ULL; }
    void wireChanged(SimulationTime t,const std::shared_ptr<WireBase>&,
                     const std::vector<LogicValue>& v) override {
        ++wires;mix(t);for(auto bit:v)mix(unsigned(bit));
    }
    void pinChanged(SimulationTime t,const std::shared_ptr<PinBase>&,
                    const std::vector<LogicValue>& v) override {
        ++pins;mix(t);for(auto bit:v)mix(unsigned(bit));
    }
    void settledThrough(SimulationTime) override {}
};

struct QueueState { uint64_t scheduled=0, done=0, limit=0, hash=0; size_t depth=0; };
class QueueEvent final : public Event {
    std::shared_ptr<QueueState> state;
public:
    QueueEvent(SimulationTime t,std::shared_ptr<QueueState> s):Event(t),state(std::move(s)){}
    EventPriority getPriority()const override{return EventPriority::WIRE_UPDATE;}
    void process(Simulator& sim) override {
        ++state->done;state->hash=state->hash*33+time;
        if(state->scheduled<state->limit) {
            ++state->scheduled;
            sim.scheduleEvent(std::make_shared<QueueEvent>(time+state->depth,state));
        }
    }
};

struct DriveState {
    std::vector<std::shared_ptr<WireBase>> wires;
    size_t round=0, rounds=0, width=1;
    bool unchanged=false;
    SimulationTime period=100;
};
class DriveEvent final : public Event {
    std::shared_ptr<DriveState> state;
public:
    DriveEvent(SimulationTime t,std::shared_ptr<DriveState> s):Event(t),state(std::move(s)){}
    EventPriority getPriority()const override{return EventPriority::WIRE_UPDATE;}
    void process(Simulator& sim) override {
        const auto bit=state->unchanged?LogicValue::LOW:LogicValue(state->round%4);
        const std::vector<LogicValue> bits(state->width,bit);
        for(const auto& wire:state->wires)sim.scheduleEvent(makeWireUpdateEvent(time,wire,bits));
        if(++state->round<state->rounds)sim.scheduleEvent(std::make_shared<DriveEvent>(time+state->period,state));
    }
};

void counters(RunRecord& r,const SimulatorPerformanceCounters& c) {
    using circuit::benchmark::addMetric;
    addMetric(r,"processed_events",c.processed_events,"events");
    addMetric(r,"wire_updates",c.processed_wire_updates,"events");
    addMetric(r,"evaluations",c.processed_component_evaluations,"events");
    addMetric(r,"wire_changes",c.effective_wire_changes,"changes");
    addMetric(r,"pin_changes",c.effective_pin_changes,"changes");
    addMetric(r,"maximum_queue",c.maximum_event_queue_depth,"events");
    addMetric(r,"events_per_second",c.processed_events/r.host_seconds,"events/s");
}

// One ELF32 executable segment, with a freestanding RV32IM loop. Contains
// multiply, divide, remainder, memory dependencies and a taken branch.
std::vector<uint8_t> guestElf(size_t iterations) {
    auto ri=[](unsigned rd,unsigned a,unsigned b,unsigned op){return 0x02000033u|(rd<<7)|(op<<12)|(a<<15)|(b<<20);};
    const int offset=-24;
    const uint32_t imm=uint32_t(offset)&0x1fff;
    const uint32_t branch=0x1063u|(5u<<15)|((imm>>12)<<31)|(((imm>>5)&63)<<25)|(((imm>>1)&15)<<8)|(((imm>>11)&1)<<7);
    std::vector<uint32_t> code={uint32_t((iterations<<20)|(5<<7)|0x13),0x00700313,0x00300393,0x00003637,
        ri(8,6,7,0),ri(9,8,7,5),ri(10,8,6,7),0x00862023,0x00062583,0xfff28293,branch,0x00100073};
    std::vector<uint8_t> bytes(0x100+code.size()*4);
    auto put=[&](size_t at,uint32_t n,size_t width){for(size_t i=0;i<width;++i)bytes[at+i]=uint8_t(n>>(i*8));};
    bytes[0]=0x7f;bytes[1]='E';bytes[2]='L';bytes[3]='F';bytes[4]=1;bytes[5]=1;bytes[6]=1;
    put(16,2,2);put(18,243,2);put(20,1,4);put(24,0,4);put(28,52,4);
    put(40,52,2);put(42,32,2);put(44,1,2);
    put(52,1,4);put(56,0x100,4);put(60,0,4);put(64,0,4);
    put(68,code.size()*4,4);put(72,code.size()*4,4);put(76,7,4);put(80,4,4);
    for(size_t i=0;i<code.size();++i)put(0x100+i*4,code[i],4);
    return bytes;
}

int main(int argc,char** argv) {
    try {
        if(argc<4||argc>6)throw std::invalid_argument("Usage: EnginePerformanceProbe queue|wire1|wire32|fanout|guest count none|digest|stream [depth-or-output] [gate|logic|visual|compact]");
        const std::string name=argv[1],trace=argv[3];
        const size_t count=std::stoull(argv[2]);
        if(!count||count>20000000)throw std::invalid_argument("Count out of bounds");
        if(trace!="none"&&trace!="digest"&&trace!="stream")throw std::invalid_argument("Invalid observation mode");
        RunRecord r;
        r.workload={"engine."+name,"engine","Bounded native engine experiment",{"host-microbenchmark"},{{"count",std::to_string(count)},{"trace",trace}}};
        r.target={"native.engine","engine",{{"fidelity",argc>5?argv[5]:"visual"}}};
        auto digest=std::make_shared<Digest>();
        std::shared_ptr<TickTraceWriter> writer;
        std::unique_ptr<circuit::visualizer::VisualizerSession> view;
        if(name=="guest") {
            if(count>1024)throw std::invalid_argument("Guest iteration cap is 1024");
            circuit::runtime::RecordedGuestOptions o;
            o.fidelity_profile=argc>5?argv[5]:"visual";
            o.max_cycles=200000;o.max_instructions=20000;o.max_events=2000000000ULL;
            if(trace!="none")o.signal_trace=[&](const auto& root,const Simulator& sim)->std::shared_ptr<SignalTraceObserver>{
                if(trace=="digest")return digest;
                if(argc<5||std::filesystem::exists(argv[4]))throw std::invalid_argument("New trace output directory required");
                std::filesystem::create_directories(argv[4]);
                view=std::make_unique<circuit::visualizer::VisualizerSession>(root,sim,"engine-experiment");
                writer=std::make_shared<TickTraceWriter>(argv[4],*view);return writer;
            };
            const auto result=circuit::runtime::runRecordedGuest(guestElf(count),{},o);
            if(writer)writer->finish();
            if(result.state.x[8]!=21||result.state.x[9]!=7||result.state.x[10]!=0||result.state.x[11]!=21)
                throw std::runtime_error("Unexpected independent guest result");
            r.host_seconds=result.execution_seconds;r.simulated_ticks=result.cycles*result.period;
            counters(r,result.events);
            circuit::benchmark::addMetric(r,"setup_seconds",result.setup_seconds,"s");
            circuit::benchmark::addMetric(r,"cycles",result.cycles,"cycles");
            circuit::benchmark::addMetric(r,"instructions",result.instructions,"instructions");
            r.fingerprints["profile"]=result.profile_fingerprint;
            uint64_t hash=0;for(auto x:result.state.x)hash=hash*33+x;
            r.fingerprints["state"]=std::to_string(hash);
        } else {
            Simulator sim;sim.setHistoryRecordingEnabled(false);
            std::unique_ptr<SignalTraceSubscription> subscription;
            if(trace=="stream")throw std::invalid_argument("Stream mode requires guest workload");
            if(trace=="digest")subscription=std::make_unique<SignalTraceSubscription>(sim,digest);
            auto root=Component::create<IOComponent>("BENCH");ComponentBuilder builder(root);
            auto queue=std::make_shared<QueueState>();
            auto drive=std::make_shared<DriveState>();
            if(name=="queue") {
                queue->limit=count;queue->depth=argc>4?std::stoull(argv[4]):64;
                if(!queue->depth||queue->depth>8192||queue->depth>count)throw std::invalid_argument("Invalid queue depth");
                for(size_t i=0;i<queue->depth;++i){++queue->scheduled;sim.scheduleEvent(std::make_shared<QueueEvent>(i,queue));}
                r.workload.parameters["depth"]=std::to_string(queue->depth);
            } else if(name=="wire1"||name=="wire32"||name=="fanout") {
                drive->rounds=count;drive->width=name=="wire32"?32:1;
                drive->unchanged=argc>4&&std::string(argv[4])=="unchanged";
                const size_t n=name=="fanout"?1:64;
                for(size_t i=0;i<n;++i) {
                    std::vector<std::shared_ptr<PinBase>> sinks;
                    if(name=="fanout")for(size_t k=0;k<64;++k) {
                        auto gate=builder.addNewComponent<NOTGate>("G"+std::to_string(k));
                        sinks.push_back(gate->getInputPin<1>("IN"));
                        builder.addNewWire("O"+std::to_string(k),gate->getOutputPin<1>("OUT"));
                    }
                    drive->wires.push_back(builder.addNewWireDynamic("W"+std::to_string(i),drive->width,nullptr,sinks));
                }
                sim.scheduleEvent(std::make_shared<DriveEvent>(1,drive));
                r.workload.parameters["unchanged"]=drive->unchanged?"true":"false";
            } else throw std::invalid_argument("Unknown workload");
            const auto start=Clock::now();
            while(sim.hasPendingEvents())sim.drainUntilIdle(MAX_SIMULATION_TIME,100000);
            r.host_seconds=std::chrono::duration<double>(Clock::now()-start).count();
            r.simulated_ticks=sim.getCurrentTime();counters(r,sim.getPerformanceCounters());
            if(name=="queue") {
                if(queue->done!=count)throw std::runtime_error("Missing queue events");
                r.fingerprints["state"]=std::to_string(queue->hash);
            } else {
                const auto expected=drive->unchanged?LogicValue::LOW:LogicValue((count-1)%4);
                for(const auto& wire:drive->wires)if(wire->getBit(0)!=expected)throw std::runtime_error("Wrong wire state");
                r.fingerprints["state"]=std::to_string(unsigned(expected));
            }
        }
        if(trace=="digest") {
            r.fingerprints["trace_digest"]=std::to_string(digest->hash);
            circuit::benchmark::addMetric(r,"observed_wires",digest->wires,"changes");
            circuit::benchmark::addMetric(r,"observed_pins",digest->pins,"changes");
        }
        if(writer)circuit::benchmark::addMetric(r,"timestamps",writer->timestampCount(),"timestamps");
        r.status=circuit::benchmark::RunStatus::Completed;r.validated=true;
        std::cout<<circuit::benchmark::toJson(r)<<'\n';
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
