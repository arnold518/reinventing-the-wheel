#include "analysis/CircuitTimingAnalyzer.hpp"
#include "../../src/analysis/MemoryTimingState.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/Gate.hpp"
#include "simulator/Simulator.hpp"
#include <iostream>
#include <stdexcept>

using namespace circuit::analysis;
using namespace circuit::memory;
using namespace circuit::memory_device;
namespace {
void check(bool v,const std::string& s){if(!v)throw std::runtime_error(s);}
struct Fixture {
    std::shared_ptr<IOComponent> root=Component::create<IOComponent>("ROOT",[](IOComponent* c){
        for(auto p:{"CLK","RST","ISSUE","TAKE"})c->addPin(p,PinType::INPUT);
        for(auto p:{"TAG","ADDR","WRITE_DATA"})c->addPin<32>(p,PinType::INPUT);
        c->addPin<3>("OP",PinType::INPUT);c->addPin<4>("WRITE_MASK",PinType::INPUT);
        c->addPin("DONE",PinType::OUTPUT);c->addPin("FAULT",PinType::OUTPUT);c->addPin<32>("DATA",PinType::OUTPUT);
    });
    std::shared_ptr<IOComponent> request;
    std::shared_ptr<BehavioralMemoryEndpoint> endpoint;
    std::map<std::string,std::shared_ptr<WireBase>> inputs,outputs;
    Simulator sim;
    ClockSpecification clock;
    size_t base=10000;
    Fixture() {
        auto built=circuit::builtinComponentCatalog().createRoot(
            {"rv32i.memory-request","REQ",{}, {}, {}, {}},
            rv32i::withBehavioralMemoryParts(circuit::maximallyStructural()));
        request=std::dynamic_pointer_cast<IOComponent>(built.root);root->addChild(request);
        endpoint=Component::create<BehavioralMemoryEndpoint>("MEM",32,Technology::FF);root->addChild(endpoint);
        endpoint->loadWords(0,{0x12345678,0x89abcdef,0x87654321,0});
        endpoint->setHistoryRecordingEnabled(false);sim.setHistoryRecordingEnabled(false);
        ComponentBuilder b(root);circuit::detail::StructuralWiring w(b);
        for(const auto& [n,p]:root->getAllInputPins()) {
            w.connect(p,request->getInputPinDynamic(n));
            if(n=="CLK"||n=="RST")w.connect(p,endpoint->getInputPinDynamic(n));
        }
        for(const auto& [n,p]:endpoint->getAllInputPins())if(n!="CLK"&&n!="RST")
            w.connect(request->getOutputPinDynamic("BUS_"+n),p);
        for(const auto& [n,p]:endpoint->getAllOutputPins())w.connect(p,request->getInputPinDynamic("BUS_"+n));
        w.connect(request->getOutputPin("DONE"),root->getOutputPin("DONE"));
        w.connect(request->getOutputPin("FAULT"),root->getOutputPin("FAULT"));
        w.connect(request->getOutputPin<32>("READ_DATA"),root->getOutputPin<32>("DATA"));w.finish();
        for(const auto& [n,p]:root->getAllInputPins())inputs[n]=b.addNewWireDynamic("drive_"+n,p->getWidth(),nullptr,{p});
        for(const auto& [n,p]:root->getAllOutputPins())outputs[n]=b.addNewWireDynamic("out_"+n,p->getWidth(),p,{});
        clock.clock_pin="ROOT.CLK";
        for(const auto& [n,p]:root->getAllInputPins())if(n!="CLK")
            clock.inputs.push_back({"ROOT."+n,n=="RST"?std::optional<ClockEdge>{}:ClockEdge::Rising,{1,1},0});
        for(const auto& [n,p]:root->getAllOutputPins())clock.outputs.push_back({"ROOT."+n,ClockEdge::Rising,0,0});
    }
    void drive(const std::string& n,size_t t,const Bits& v) {
        const auto& w=inputs.at(n);
        auto add=[&]<size_t W>(){sim.scheduleEvent(std::make_shared<WireUpdateEvent<W>>(t,std::dynamic_pointer_cast<Wire<W>>(w),v));};
        switch(v.size()){case 1:add.template operator()<1>();break;case 3:add.template operator()<3>();break;
            case 4:add.template operator()<4>();break;case 32:add.template operator()<32>();break;default:throw std::runtime_error("Bad width");}
    }
    void initialize() {
        for(const auto& [n,w]:inputs)drive(n,0,bits(w->getWidth(),n=="RST"?1:0));
        // Constants must be initialized through the same hooks as normal tests.
        auto init=[&](auto&& self,const std::shared_ptr<Component>& c)->void{
            if(auto basic=std::dynamic_pointer_cast<BasicComponent>(c))basic->scheduleInitialEvents(sim,0);
            for(const auto& child:c->getChildren())self(self,child);
        };init(init,root);sim.runAndRecord(4000);drive("RST",4001,{L});sim.runAndRecord(8000);
    }
    std::vector<Bits> frame(size_t period,unsigned controls,unsigned serial) {
        drive("CLK",base,{H});drive("CLK",base+period/2,{L});
        drive("ISSUE",base+1,{LogicValue(controls%4)});drive("TAKE",base+1,{LogicValue((controls/4)%4)});
        auto tag=bits(32,serial%3);if(controls/16==2)tag.assign(32,X);if(controls/16==3)tag.assign(32,Z);
        drive("TAG",base+1,tag);drive("ADDR",base+1,bits(32,4*(serial%5)));
        drive("OP",base+1,bits(3,(serial/5)%4));drive("WRITE_DATA",base+1,bits(32,0x12340000u+serial));
        drive("WRITE_MASK",base+1,bits(4,serial%16));
        sim.runAndRecord(base+period-1);
        check(!sim.hasPendingEvents(),"Request/response circuit did not drain at computed period");
        std::vector<Bits> result;
        for(const auto& [n,w]:outputs)result.push_back(w->getValueVector());
        for(const auto& [n,p]:request->getAllOutputPins()) {
            auto wire=p->getExternalWireBase();if(wire)result.push_back(wire->getValueVector());
        }
        for(const auto& child:request->getChildren())if(child->getName()=="BUSY"||child->getName()=="SENT"||child->getName()=="ABANDONED") {
            auto io=std::dynamic_pointer_cast<IOComponent>(child);
            result.push_back(io->getOutputPin("Q")->getExternalWireBase()->getValueVector());
        }
        result.push_back(endpoint->memoryStorage().readLogic(0,16));
        base+=period;return result;
    }
};
void run() {
    // Binary enumeration records its input premises; proving only the storage
    // captures would not establish that ISSUE/TAKE/TAG_MATCH are binary.
    {
        Fixture binary_fixture;
        binary_fixture.endpoint->clearContents();
        auto binary=binary_fixture.clock;binary.binary_startup=true;
        for(auto& input:binary.inputs)if(input.edge)input.binary_values=true;
        const auto binary_states=detail::memoryTimingStates(binary_fixture.endpoint,binary);
        check(binary_states&&binary_states->cases.size()==2&&binary_states->binary_controls.size()==3,
            "Binary reachability must expose all quantified control premises");
        const auto binary_report=CircuitTimingAnalyzer::safeClock(binary_fixture.root,binary);
        check(binary_report.status==ClockTimingStatus::SafeUnderContracts&&binary_report.binary_startup_verified,
            "Binary requester control premises must be provable");
        bool controls_checked=false;
        for(const auto& e:binary_report.binary_evidence)
            controls_checked|=e.find("3 quantified control inputs checked")!=std::string::npos;
        check(controls_checked,"Binary control/capture timing must be checked");
    }
    Fixture fast,reference;
    const auto proof=detail::memoryTimingStates(fast.endpoint,fast.clock);
    check(proof && proof->cases.size()==6,"Four-state reachability must not use the binary-only two-state proof");
    bool saw_unknown=false;
    for(const auto& c:proof->cases)for(const auto& [p,v]:c.registers) {
        saw_unknown|=v==X;
        if(p->getOwner()->getName()=="SENT"&&c.full)check(v!=L,"Full response with definite unsent is unreachable");
    }
    check(saw_unknown,"Unknown registered control states must be included");
    const auto report=CircuitTimingAnalyzer::safeClock(fast.root,fast.clock);
    for(const auto& d:report.diagnostics)std::cerr<<d<<'\n';
    check(report.status==ClockTimingStatus::SafeUnderContracts && report.checked_state_cases==6,"Expected complete state-aware bound");
    auto bounded=fast.clock;bounded.maximum_state_cases=1;
    auto bad=CircuitTimingAnalyzer::safeClock(fast.root,bounded);
    check(bad.status==ClockTimingStatus::Unknown&&!bad.safe_period_ticks,"State enumeration budget must fail closed");
    bounded=fast.clock;bounded.maximum_state_evaluations=1;
    bad=CircuitTimingAnalyzer::safeClock(fast.root,bounded);
    check(bad.status==ClockTimingStatus::Unknown&&!bad.safe_period_ticks,"State settling work budget must fail closed");
    auto held=fast.clock;held.outputs[0].hold=1000;
    bad=CircuitTimingAnalyzer::safeClock(fast.root,held);
    check(bad.status==ClockTimingStatus::TimingViolation&&!bad.safe_period_ticks,"Mode simplification must not hide hold violations");
    fast.initialize();reference.initialize();
    for(unsigned pass=0;pass<4;++pass)for(unsigned controls=0;controls<64;++controls) {
        const auto serial=pass*64+controls;
        check(fast.frame(*report.safe_period_ticks,controls,serial)==reference.frame(2000,controls,serial),
            "Bounded-period requester differs from long-period reference at frame "+std::to_string(serial));
    }
    // Unrelated oscillators must remain unknown even when a memory proof exists.
    ComponentBuilder b(fast.root);auto inv=b.addNewComponent<NOTGate>("OSCILLATOR");
    b.addNewWire("feedback",inv->getOutputPin("OUT"),{inv->getInputPin("IN")});
    bad=CircuitTimingAnalyzer::safeClock(fast.root,fast.clock);
    check(bad.status==ClockTimingStatus::Unknown&&!bad.safe_period_ticks&&!bad.dependency_cycle.empty(),"Unresolved cycles must fail closed");
    std::cout<<"6 reachable states, 384 four-state transitions; 256 coupled circuit frames at period "
        <<*report.safe_period_ticks<<" matched a 2000-tick reference\n";
}
}
int main(){try{run();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
