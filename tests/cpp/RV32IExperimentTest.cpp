#include "analysis/RV32IExperiment.hpp"
#include "analysis/ExperimentClock.hpp"
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/rv32i/RegisterFile32x32.hpp"
#include "modules/utility/Rewire.hpp"
#include <iostream>
#include <limits>

using namespace circuit::analysis;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F> void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,"Expected rejection");}
class Unmodeled : public BasicComponent {
public:
    Unmodeled(std::string name):BasicComponent(std::move(name),1,[](IOComponent* s){
        s->addPin("CLK",PinType::INPUT);s->addPin("D",PinType::INPUT);s->addPin("Q",PinType::OUTPUT);
    }) {}
    void evaluate(size_t,Simulator&) override {}
};
int main() {
    try {
        ExperimentClock clock(100);
        check(clock.edge(0)==301&&clock.edge(4)==701&&clock.falling(0)==351&&clock.end(0)==400,"Absolute clock");
        rejects([]{ExperimentClock c(0);});rejects([]{ExperimentClock c(5);});
        rejects([&]{clock.edge(std::numeric_limits<size_t>::max());});
        auto opaque=Component::create<Unmodeled>("OPAQUE");
        check(!CircuitTimingAnalyzer::settling(opaque).valid,"Unknown sequential module must fail closed");
        auto loop=Component::create<IOComponent>("LOOP");ComponentBuilder lb(loop);
        auto inv=lb.addNewComponent<NOTGate>("INV");
        lb.addNewWire("loop",inv->getOutputPin("OUT"),{inv->getInputPin("IN")});
        check(!CircuitTimingAnalyzer::settling(loop).valid,"Self loop");
        auto mapped=Component::create<IOComponent>("MAPPED");ComponentBuilder mb(mapped);
        auto rw=mb.addNewComponent<Rewire>("RW",std::vector<Rewire::WireSpec>{{"A",1},{"B",1}},
            std::vector<Rewire::WireSpec>{{"X",1},{"Y",1}},
            std::vector<Rewire::BitMap>{{"A",0,"X",0},{"B",0,"Y",0}});
        mb.addNewWire("chain",rw->getOutputPin("X"),{rw->getInputPin("B")});
        check(CircuitTimingAnalyzer::settling(mapped).valid,"Independent Rewire arcs must not create a loop");
        auto rf=circuit::builtinComponentCatalog().createRoot(
            circuit::families::RegisterFile32x32.request("RF"),circuit::strictAllBehavioral()).root;
        SettlingOptions reads;reads.sources=SettlingSources::InputsOnly;reads.fixed_inputs["RST"]=0;
        const auto r=CircuitTimingAnalyzer::settling(rf,reads);
        size_t read_arcs=0;
        // 12 ticks: the read path proven on the gate-level register file.
        for(const auto& e:r.endpoints)read_arcs+=e.changes&&e.bound==12&&(e.name=="RS1_DATA"||e.name=="RS2_DATA");
        check(r.valid&&read_arcs==2,"Both asynchronous register read arcs remain visible");
        // A compact system is analyzed from its proven delay tables; it is
        // never certified faster than its gates.
        RV32IExperimentConfig config;
        for(const auto* core:{"single","baseline"}) {
            auto compact=config;compact.core=core;compact.safe_clock_only=true;
            auto gates=compact;gates.fidelity="reference";
            const auto compact_clock=runRV32IExperiment(compact).safe_clock,gate_clock=runRV32IExperiment(gates).safe_clock;
            check(compact_clock.status==ClockTimingStatus::SafeUnderContracts&&gate_clock.safe_period_ticks
                  &&compact_clock.safe_period_ticks&&*compact_clock.safe_period_ticks>=*gate_clock.safe_period_ticks,
                  "Compact system clock is proven and not below the gates'");
        }
        for(const auto* core:{"single","baseline","advanced"}) {
            config.core=core;
            auto run=runRV32IExperiment(config);
            check(run.verified&&run.attempts==51&&run.retired==50&&run.terminal_attempts==1,"Common retirement definitions");
            check(run.cycles==(config.core=="single"?51:config.core=="baseline"?138:85),"Cycle golden");
            auto slower=config;slower.period=8000;
            auto second=runRV32IExperiment(slower);
            check(second.cycles==run.cycles&&second.simulator.processed_events==run.simulator.processed_events,"Idle ticks must not change work");
        }
        config.core="advanced";config.max_events=10;
        rejects([&]{runRV32IExperiment(config);});
        std::cout<<"Experiment clock, timing arcs, budgets and cross-core metrics passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
