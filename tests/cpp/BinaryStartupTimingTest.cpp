#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/utility/Rewire.hpp"
#include "../../src/modules/memory/FlipFlopCellBehavioral.hpp"
#include "simulator/Simulator.hpp"
#include "simulator/SimulationTest.hpp"
#include <iostream>
#include <stdexcept>

using namespace circuit::analysis;
using namespace circuit::memory;
using namespace circuit::memory_device;
namespace {
void check(bool v,const std::string& s){if(!v)throw std::runtime_error(s);}
void unknown(const ClockTimingReport& r,const std::string& reason) {
    check(r.status==ClockTimingStatus::Unknown&&!r.safe_period_ticks&&!r.binary_startup_verified,"Unsafe binary certification: "+reason);
    bool found=false;for(const auto& d:r.diagnostics)found|=d.find(reason)!=std::string::npos;
    check(found,"Missing diagnostic: "+reason);
}
void images() {
    for(bool paged:{false,true}) {
        MemoryStorage s(1025,32,X,false,{paged,65536,65536});
        check(!s.isFullyBinary(),"Unknown default image");
        s.loadBytes(0,std::vector<uint8_t>(4096,0));
        check(!s.isFullyBinary(),"Uninitialized last word must be checked");
        s.loadBytes(4096,{1,2,3,4});
        check(s.isFullyBinary(),"Fully provisioned image, ignoring unused last-page padding");
        s.writeLogic(0,13,Bits(8,Z),false);
        check(!s.isFullyBinary(),"Z written as X must invalidate binary image");
        s.clearContents();check(s.isFullyBinary(),"Clear establishes known zero fill");
    }
    MemoryStorage huge(128*1024*1024,32,L,false,{true,65536,65536});
    check(huge.isFullyBinary()&&huge.allocatedDataBytes()==0,"Sparse image check must not materialize 512 MiB");
    huge.writeLogic(0,huge.capacityBytes()-1,Bits(8,X),false);
    check(!huge.isFullyBinary()&&huge.allocatedDataBytes()<16384,"Sparse last-page unknown must be found");
}
void endpoint() {
    auto c=Component::create<BehavioralMemoryEndpoint>("MEM",32,Technology::FF);
    ClockSpecification clock;clock.clock_pin="MEM.CLK";clock.binary_startup=true;
    for(const auto& [n,p]:c->getAllInputPins())if(n!="CLK") {
        if(n=="RST"||n=="RSP_READY")clock.inputs.push_back({"MEM."+n,std::nullopt,{},n=="RST"?0u:1u});
        else clock.inputs.push_back({"MEM."+n,ClockEdge::Rising,{1,7},0,true});
    }
    for(const auto& [n,p]:c->getAllOutputPins())clock.outputs.push_back({"MEM."+n,ClockEdge::Rising,0,0});
    unknown(CircuitTimingAnalyzer::safeClock(c,clock),"initial memory image contains");
    c->clearContents();c->initializeBytes(3,{0x12,0x34});
    const auto r=CircuitTimingAnalyzer::safeClock(c,clock);
    for(const auto& d:r.diagnostics)std::cerr<<d<<'\n';
    check(r.status==ClockTimingStatus::SafeUnderContracts&&r.binary_startup_verified&&r.binary_capture_bits>0,"Binary FF endpoint must be certifiable");
    auto unsafe=clock;
    for(auto& i:unsafe.inputs)if(i.edge){i.binary_values=false;break;}
    unknown(CircuitTimingAnalyzer::safeClock(c,unsafe),"binary_values");
    unsafe=clock;unsafe.maximum_state_evaluations=1;
    unknown(CircuitTimingAnalyzer::safeClock(c,unsafe),"budget");
    unsafe=clock;unsafe.outputs.front().hold=100;
    auto hold=CircuitTimingAnalyzer::safeClock(c,unsafe);
    check(hold.status==ClockTimingStatus::TimingViolation&&!hold.binary_startup_verified&&!hold.safe_period_ticks,"Binary proof must not conceal hold violations");
    Simulator sim;c->evaluate(0,sim);
    unknown(CircuitTimingAnalyzer::safeClock(c,clock),"pre-simulation");
}
void captures(bool poison) {
    auto root=Component::create<IOComponent>("ROOT",[](IOComponent* c){
        for(auto p:{"CLK","RST","DATA"})c->addPin(p,PinType::INPUT);
        c->addPin("Q",PinType::OUTPUT);
    });
    ComponentBuilder b(root);circuit::detail::StructuralWiring w(b);
    auto ff=b.addNewComponent<FlipFlopCellBehavioral>("FF");
    for(auto p:{"CLK","RST"})w.connect(root->getInputPin(p),ff->getInputPin(p));
    w.connect(w.constant("ENABLE",true),ff->getInputPin("WE"));
    if(poison) {
        auto x=b.addNewComponent<Rewire>("X_SOURCE",std::vector<Rewire::WireSpec>{},
            std::vector<Rewire::WireSpec>{{"OUT",1}},std::vector<Rewire::BitMap>{},Rewire::UnmappedBitValue::UNKNOWN);
        w.connect(x->getOutputPin("OUT"),ff->getInputPin("D"));
    } else w.connect(root->getInputPin("DATA"),ff->getInputPin("D"));
    w.connect(ff->getOutputPin("Q"),root->getOutputPin("Q"));w.finish();
    ClockSpecification clock;clock.clock_pin="ROOT.CLK";clock.binary_startup=true;
    clock.inputs={{"ROOT.RST",std::nullopt,{},0},{"ROOT.DATA",ClockEdge::Rising,{1,2},0,true}};
    clock.outputs={{"ROOT.Q",ClockEdge::Rising,0,0}};
    auto r=CircuitTimingAnalyzer::safeClock(root,clock);
    if(poison)unknown(r,"binary next-state preservation unproved");
    else {
        for(const auto& d:r.diagnostics)std::cerr<<d<<'\n';
        check(r.status==ClockTimingStatus::SafeUnderContracts&&r.binary_startup_verified,"Binary register induction");
        clock.inputs[1].edge=ClockEdge::Falling;
        unknown(CircuitTimingAnalyzer::safeClock(root,clock),"rising-edge launches");
    }
}
void controllingFeedback(bool dual) {
    auto root=Component::create<IOComponent>("ROOT",[](IOComponent* c){
        for(auto p:{"CLK","RST","A"})c->addPin(p,PinType::INPUT);
        c->addPin("Y",PinType::OUTPUT);c->addPin("Q",PinType::OUTPUT);
    });
    ComponentBuilder b(root);circuit::detail::StructuralWiring w(b);
    auto reg=b.addNewComponent<FlipFlopCellBehavioral>("FF");
    std::shared_ptr<IOComponent> inner,outer;
    if(dual){inner=b.addNewComponent<ORGate>("INNER");outer=b.addNewComponent<ANDGate>("OUTER");}
    else {inner=b.addNewComponent<ANDGate>("INNER");outer=b.addNewComponent<ORGate>("OUTER");}
    auto inv=b.addNewComponent<NOTGate>("INVERT");
    w.connect(root->getInputPin("A"),inner->getInputPin("A"));
    w.connect(root->getInputPin("A"),outer->getInputPin("A"));
    w.connect(inner->getOutputPin("OUT"),outer->getInputPin("B"));
    w.connect(outer->getOutputPin("OUT"),inv->getInputPin("IN"));
    w.connect(inv->getOutputPin("OUT"),inner->getInputPin("B"));
    w.connect(outer->getOutputPin("OUT"),reg->getInputPin("D"));
    w.connect(outer->getOutputPin("OUT"),root->getOutputPin("Y"));
    w.connect(reg->getOutputPin("Q"),root->getOutputPin("Q"));
    for(auto p:{"CLK","RST"})w.connect(root->getInputPin(p),reg->getInputPin(p));
    w.connect(w.constant("EN",true),reg->getInputPin("WE"));w.finish();
    std::map<std::string,std::shared_ptr<Wire<1>>> inputs,outputs;
    for(const auto& [n,p]:root->getAllInputPins())inputs[n]=b.addNewWire("drive_"+n,nullptr,{std::dynamic_pointer_cast<Pin<1>>(p)});
    for(const auto& [n,p]:root->getAllOutputPins())outputs[n]=b.addNewWire("observe_"+n,std::dynamic_pointer_cast<Pin<1>>(p),{});
    ClockSpecification clock;clock.clock_pin="ROOT.CLK";clock.binary_startup=true;
    clock.inputs={{"ROOT.RST",std::nullopt,{},0},{"ROOT.A",ClockEdge::Rising,{1,5},0,true}};
    clock.outputs={{"ROOT.Y",ClockEdge::Rising,0,0},{"ROOT.Q",ClockEdge::Rising,0,0}};
    const auto proof=CircuitTimingAnalyzer::safeClock(root,clock);
    for(const auto& d:proof.diagnostics)std::cerr<<d<<'\n';
    check(proof.status==ClockTimingStatus::SafeUnderContracts&&proof.binary_startup_verified,"Controlling-value feedback must be provable");
    bool evidence=false;for(const auto& s:proof.state_evidence)evidence|=s.find("controlling-value")!=std::string::npos;
    check(evidence,"Feedback proof must record its controlling-value rule");
    Simulator sim;sim.setHistoryRecordingEnabled(false);
    auto drive=[&](const std::string& n,size_t t,LogicValue value){sim.scheduleEvent(std::make_shared<WireUpdateEvent<1>>(t,inputs[n],value));};
    for(const auto& [n,p]:inputs)drive(n,0,n=="RST"?H:L);
    SimulationTest::scheduleInitialEventsForTree(root,sim,0);sim.runAndRecord(100);
    drive("RST",101,L);sim.runAndRecord(200);
    const auto period=*proof.safe_period_ticks;LogicValue previous=L;
    for(size_t i=0;i<64;++i) {
        const size_t t=1000+i*period;const auto value=(i^(i/3))&1?H:L;
        drive("CLK",t,H);drive("CLK",t+period/2,L);drive("A",t+5,value);
        sim.runAndRecord(t+period-1);
        // Gates may still have same-value publications queued on internal
        // paths. A capture deadline bounds wire transitions at the captures,
        // not global queue emptiness (which is not required between edges).
        check(outputs["Y"]->getValueVector()==Bits{value}&&outputs["Q"]->getValueVector()==Bits{previous},"Feedback/capture values differ at computed clock");
        previous=value;
    }
    sim.runAndRecord(1000+64*period+100);
    check(!sim.hasPendingEvents(),"Frozen binary feedback must settle and drain");
    std::cout<<(dual?"AND/OR":"OR/AND")<<" absorption: 64 timed frames at "<<period<<" ticks\n";
}
}
int main(){try{images();endpoint();captures(false);captures(true);controllingFeedback(false);controllingFeedback(true);
    std::cout<<"Binary startup images, sparse bounds, input premises, capture preservation, hold and budget rejection passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
