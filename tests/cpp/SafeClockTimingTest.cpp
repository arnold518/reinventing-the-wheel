#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/storage/Register.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/rv32i/RegisterFile32x32.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace circuit::analysis;
namespace {
void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
bool mentions(const ClockTimingReport& r, const std::string& text) {
    for (const auto& d : r.diagnostics) if (d.find(text) != std::string::npos) return true;
    return false;
}
void safe(const ClockTimingReport& r, size_t period) {
    for (const auto& d : r.diagnostics) std::cerr << d << '\n';
    check(r.status == ClockTimingStatus::SafeUnderContracts && r.safe_period_ticks == period,
          "Expected safe period " + std::to_string(period));
}
void unknown(const ClockTimingReport& r, const std::string& text) {
    if (r.status != ClockTimingStatus::Unknown || r.safe_period_ticks || !mentions(r, text))
        for (const auto& d : r.diagnostics) std::cerr << d << '\n';
    check(r.status == ClockTimingStatus::Unknown && !r.safe_period_ticks && mentions(r, text),
          "Expected Unknown containing: " + text);
}
// Test-only black box. It is never accepted without an explicit caller contract.
class TestRegister : public BasicComponent {
public:
    TestRegister(std::string name) : BasicComponent(std::move(name), 2, [](IOComponent* c) {
        c->addPin("CLK", PinType::INPUT); c->addPin("D", PinType::INPUT); c->addPin("Q", PinType::OUTPUT);
    }) {}
    void evaluate(size_t, Simulator&) override {}
};
class UnmodeledGate : public NOTGate {
public:
    using NOTGate::NOTGate;
    void evaluate(size_t, Simulator&) override {}
};
struct Fixture {
    std::shared_ptr<IOComponent> root = Component::create<IOComponent>("ROOT", [](IOComponent* c) {
        c->addPin("CLK", PinType::INPUT); c->addPin("DATA", PinType::INPUT);
    });
    ComponentBuilder b{root};
    std::shared_ptr<TestRegister> launch = b.addNewComponent<TestRegister>("L");
    std::shared_ptr<TestRegister> capture = b.addNewComponent<TestRegister>("C");
    ClockSpecification clock;
    Fixture(bool reconverge = false) {
        b.addNewWire("clk", root->getInputPin("CLK"), {launch->getInputPin("CLK"), capture->getInputPin("CLK")});
        b.addNewWire("data", root->getInputPin("DATA"), {launch->getInputPin("D")});
        auto merge = reconverge ? b.addNewComponent<ORGate>("MERGE") : nullptr;
        auto previous = launch->getOutputPin("Q");
        for (unsigned i = 0; i < 3; ++i) {
            auto gate = b.addNewComponent<NOTGate>("G" + std::to_string(i));
            std::vector<std::shared_ptr<Pin<>>> sinks{gate->getInputPin("IN")};
            if (merge && i == 0) sinks.push_back(merge->getInputPin("B"));
            b.addNewWire("chain" + std::to_string(i), previous, sinks);
            previous = gate->getOutputPin("OUT");
        }
        if (merge) {
            b.addNewWire("slow", previous, {merge->getInputPin("A")});
            previous = merge->getOutputPin("OUT");
        }
        b.addNewWire("capture", previous, {capture->getInputPin("D")});
        clock.clock_pin = "ROOT.CLK";
        clock.inputs = {{"ROOT.DATA", ClockEdge::Rising, {1, 1}, 0}};
    }
};
TimingContracts registerContracts(ClockEdge edge = ClockEdge::Rising, size_t hold = 0, size_t pulse = 1) {
    return {[=](const IOComponent& c) -> std::optional<ClockComponentContract> {
        if (!dynamic_cast<const TestRegister*>(&c)) return std::nullopt;
        ClockComponentContract m;
        m.evidence = "test fixture contract"; m.clock_pin = "CLK";
        const auto e = c.getName() == "C" ? edge : ClockEdge::Rising;
        m.captures = {{"D", e, 3, c.getName() == "C" ? hold : 0}};
        m.launches = {{"Q", e, {2, 2}}};
        m.minimum_high_ticks = pulse; m.minimum_low_ticks = 1;
        return m;
    }};
}
void boundsAndFailures() {
    Fixture f;
    auto full = CircuitTimingAnalyzer::safeClock(f.root, f.clock, registerContracts());
    safe(full, 10); // 2 launch + 3 gates + 3 setup + 1 separation, rounded even.
    check(full.critical_path.front() == "ROOT.L.output.Q" && full.critical_path.back() == "ROOT.C.input.D", "Critical path endpoints");
    check(full.checked_setup_relationships == 2 && full.checked_hold_relationships == 2, "Both captures checked");
    safe(CircuitTimingAnalyzer::safeClock(f.root, f.clock, registerContracts(ClockEdge::Falling)), 18);
    safe(CircuitTimingAnalyzer::safeClock(f.root, f.clock, registerContracts(ClockEdge::Falling, 30)), 52);
    safe(CircuitTimingAnalyzer::safeClock(f.root, f.clock, registerContracts(ClockEdge::Rising, 0, 17)), 34);
    auto bad_hold = CircuitTimingAnalyzer::safeClock(f.root, f.clock, registerContracts(ClockEdge::Rising, 6));
    check(bad_hold.status == ClockTimingStatus::TimingViolation && !bad_hold.safe_period_ticks
          && mentions(bad_hold, "hold bound"), "Hold violation must not yield a period");
    unknown(CircuitTimingAnalyzer::safeClock(f.root, f.clock), "no all-state timing contract");
    auto missing = f.clock; missing.inputs.clear();
    unknown(CircuitTimingAnalyzer::safeClock(f.root, missing, registerContracts()), "unconstrained");
    auto too_small = f.clock; too_small.maximum_nodes = 2;
    unknown(CircuitTimingAnalyzer::safeClock(f.root, too_small, registerContracts()), "budget");
    too_small = f.clock; too_small.maximum_edges = 1;
    unknown(CircuitTimingAnalyzer::safeClock(f.root, too_small, registerContracts()), "budget");
    too_small = f.clock; too_small.maximum_hierarchy_depth = 1;
    unknown(CircuitTimingAnalyzer::safeClock(f.root, too_small, registerContracts()), "depth budget");
    auto invalid = f.clock; invalid.inputs[0].delay = {4, 1};
    unknown(CircuitTimingAnalyzer::safeClock(f.root, invalid, registerContracts()), "Reversed");
    invalid = f.clock; invalid.inputs.push_back(invalid.inputs[0]);
    unknown(CircuitTimingAnalyzer::safeClock(f.root, invalid, registerContracts()), "Duplicate");
    invalid = f.clock; invalid.inputs[0].pin = "ROOT.C.D";
    unknown(CircuitTimingAnalyzer::safeClock(f.root, invalid, registerContracts()), "internally driven");
    invalid = f.clock; invalid.inputs[0].delay.maximum = std::numeric_limits<size_t>::max();
    unknown(CircuitTimingAnalyzer::safeClock(f.root, invalid, registerContracts()), "overflow");
    invalid = f.clock; invalid.event_separation_ticks = 0;
    unknown(CircuitTimingAnalyzer::safeClock(f.root, invalid, registerContracts()), "Invalid");
    invalid = f.clock; invalid.inputs[0].edge = static_cast<ClockEdge>(99);
    unknown(CircuitTimingAnalyzer::safeClock(f.root, invalid, registerContracts()), "Invalid clock edge");
    unknown(CircuitTimingAnalyzer::safeClock({}, f.clock), "Invalid");

    auto incomplete = registerContracts();
    const auto original = incomplete.model;
    incomplete.model = [=](const IOComponent& c) {
        auto m = original(c); if (m) m->captures.clear(); return m;
    };
    unknown(CircuitTimingAnalyzer::safeClock(f.root, f.clock, incomplete), "input absent");
    incomplete.model = [=](const IOComponent& c) {
        auto m = original(c); if (m) m->evidence.clear(); return m;
    };
    unknown(CircuitTimingAnalyzer::safeClock(f.root, f.clock, incomplete), "evidence");
    incomplete.model = [=](const IOComponent& c) {
        auto m = original(c); if (m) m->arcs.push_back({"CLK", "Q", {0, 1}}); return m;
    };
    unknown(CircuitTimingAnalyzer::safeClock(f.root, f.clock, incomplete), "clock/unused");
    incomplete.model = [=](const IOComponent& c) {
        auto m = original(c); if (m) m->stable_outputs.push_back("Q"); return m;
    };
    unknown(CircuitTimingAnalyzer::safeClock(f.root, f.clock, incomplete), "contradictory");
    incomplete.model = [=](const IOComponent& c) {
        auto m = original(c); if (m) m->unused_inputs.push_back("CLK"); return m;
    };
    unknown(CircuitTimingAnalyzer::safeClock(f.root, f.clock, incomplete), "clock pin cannot be unused");
    incomplete.model = [=](const IOComponent& c) {
        auto m = original(c); if (m) m->fixed_inputs["D"] = 2; return m;
    };
    unknown(CircuitTimingAnalyzer::safeClock(f.root, f.clock, incomplete), "exceeds pin width");
}
void cyclesAndClocks() {
    Fixture reconvergent(true);
    safe(CircuitTimingAnalyzer::safeClock(reconvergent.root, reconvergent.clock, registerContracts()), 10);
    const auto fast_hold = CircuitTimingAnalyzer::safeClock(reconvergent.root, reconvergent.clock,
        registerContracts(ClockEdge::Rising, 3));
    check(fast_hold.status == ClockTimingStatus::TimingViolation && !fast_hold.safe_period_ticks,
          "Hold must follow the shortest reconvergent path, not the longest setup path");
    check(fast_hold.limiting_constraint.find("same-edge hold") != std::string::npos
          && fast_hold.critical_path.size() == 4, "Hold report must identify the short path");

    Fixture loop;
    auto inv = loop.b.addNewComponent<NOTGate>("OSCILLATOR");
    loop.b.addNewWire("feedback", inv->getOutputPin("OUT"), {inv->getInputPin("IN")});
    const auto cyclic=CircuitTimingAnalyzer::safeClock(loop.root, loop.clock, registerContracts());
    unknown(cyclic, "dependency cycle");
    check(cyclic.dependency_cycle.size()==3 && cyclic.dependency_cycle.front()==cyclic.dependency_cycle.back(),
          "Cycle report must be an actual closed walk, not a list of downstream pins");
    for(const auto& p:cyclic.dependency_cycle)check(p.find("OSCILLATOR")!=std::string::npos,"Wrong cycle member");

    Fixture opaque;
    opaque.b.addNewComponent<UnmodeledGate>("LOOKS_LIKE_NOT");
    unknown(CircuitTimingAnalyzer::safeClock(opaque.root, opaque.clock, registerContracts()), "no all-state timing contract");

    Fixture gated;
    auto gate = gated.b.addNewComponent<NOTGate>("CLOCK_GATE");
    gated.b.addNewWire("clock_data", gated.root->getInputPin("CLK"), {gate->getInputPin("IN")});
    gated.b.addNewWire("gated", gate->getOutputPin("OUT"), {gated.capture->getInputPin("CLK")});
    unknown(CircuitTimingAnalyzer::safeClock(gated.root, gated.clock, registerContracts()), "clock");

    Fixture physical;
    physical.b.addNewComponent<DFlipFlop>("GATE_DFF");
    // The real master/slave latch feedback must not inherit the direct FF model.
    unknown(CircuitTimingAnalyzer::safeClock(physical.root, physical.clock, registerContracts()), "unconstrained");
}
void externalOutputsAndRewire() {
    auto root = Component::create<IOComponent>("RW", [](IOComponent* c) {
        c->addPin("CLK", PinType::INPUT); c->addPin("D", PinType::INPUT); c->addPin("Q", PinType::OUTPUT);
    });
    ComponentBuilder b(root);
    auto rw = b.addNewComponent<Rewire>("MAPPING", std::vector<Rewire::WireSpec>{{"A",1},{"B",1}},
        std::vector<Rewire::WireSpec>{{"X",1},{"Y",1}},
        std::vector<Rewire::BitMap>{{"A",0,"X",0},{"B",0,"Y",0}});
    b.addNewWire("d", root->getInputPin("D"), {rw->getInputPin("A")});
    b.addNewWire("chain", rw->getOutputPin("X"), {rw->getInputPin("B")});
    b.addNewWire("q", rw->getOutputPin("Y"), {root->getOutputPin("Q")});
    ClockSpecification clock; clock.clock_pin = "RW.CLK";
    clock.inputs = {{"RW.D", ClockEdge::Rising, {1,2}, 0}};
    unknown(CircuitTimingAnalyzer::safeClock(root, clock), "no sampling requirement");
    clock.outputs = {{"RW.Q",ClockEdge::Rising,1,0}};
    safe(CircuitTimingAnalyzer::safeClock(root, clock), 4);
}
void builtinStorage() {
    const auto& catalog = circuit::builtinComponentCatalog();
    auto reg = catalog.createRoot(circuit::families::registerWord(32).request("REG"), circuit::strictAllBehavioral()).root;
    ClockSpecification clock; clock.clock_pin = "REG.input.CLK";
    clock.inputs = {{"REG.input.RST",std::nullopt,{},0}, {"REG.input.WE",std::nullopt,{},1},
                    {"REG.input.D",ClockEdge::Rising,{1,3},0}};
    clock.outputs = {{"REG.output.Q",ClockEdge::Rising,0,0}};
    safe(CircuitTimingAnalyzer::safeClock(reg, clock), 4);
    clock.inputs[0].fixed_value = 1;
    unknown(CircuitTimingAnalyzer::safeClock(reg, clock), "requires fixed value 0");

    auto rf = catalog.createRoot(circuit::families::RegisterFile32x32.request("RF"), circuit::strictAllBehavioral()).root;
    clock = {}; clock.clock_pin = "RF.CLK";
    clock.inputs = {{"RF.RST",std::nullopt,{},0}, {"RF.REG_WRITE",std::nullopt,{},1},
        {"RF.RD_ADDR",ClockEdge::Rising,{1,1},0}, {"RF.WRITE_DATA",ClockEdge::Rising,{1,1},0},
        {"RF.RS1_ADDR",ClockEdge::Rising,{1,9},0}, {"RF.RS2_ADDR",ClockEdge::Rising,{1,2},0}};
    clock.outputs = {{"RF.RS1_DATA",ClockEdge::Rising,0,0},{"RF.RS2_DATA",ClockEdge::Rising,0,0}};
    safe(CircuitTimingAnalyzer::safeClock(rf, clock), 12); // Read-address arc must not be cut at the clocked macro.
}

// Dynamic cross-check of a lower-level FullAdder between compact FF cells.
// Exhaust all 8 operand/carry combinations at the calculated period. This is a
// regression of the bound; the static contracts, not these vectors, supply it.
void structuralAdder() {
    auto root = Component::create<IOComponent>("ADDER", [](IOComponent* c) {
        for (auto name : {"CLK","RST","A","B","C"}) c->addPin(name,PinType::INPUT);
    });
    ComponentBuilder b(root);
    auto fa = b.addNewComponent<FullAdder>("FA");
    std::vector<std::shared_ptr<IOComponent>> regs;
    auto one = b.addNewComponent<ConstantValue<1>>("ONE",1);
    for (auto name : {"A","B","C","S","CO"}) {
        auto part = circuit::builtinComponentCatalog().createRoot(
            circuit::families::FlipFlopCell.request(name), circuit::strictAllBehavioral()).root;
        root->addChild(part); regs.push_back(std::dynamic_pointer_cast<IOComponent>(part));
    }
    std::vector<std::shared_ptr<Pin<>>> clks, resets, enables;
    for (const auto& r : regs) {clks.push_back(r->getInputPin("CLK"));resets.push_back(r->getInputPin("RST"));enables.push_back(r->getInputPin("WE"));}
    b.addNewWire("clocks",root->getInputPin("CLK"),clks);
    b.addNewWire("resets",root->getInputPin("RST"),resets);
    b.addNewWire("enables",one->getOutputPin("OUT"),enables);
    const std::vector<std::string> names{"A","B","C"}, fa_pins{"A","B","Carry_in"};
    for (size_t i=0;i<3;++i) {
        b.addNewWire("operand"+names[i],root->getInputPin(names[i]),{regs[i]->getInputPin("D")});
        b.addNewWire("ff"+names[i],regs[i]->getOutputPin("Q"),{fa->getInputPin(fa_pins[i])});
    }
    b.addNewWire("sum",fa->getOutputPin("Sum"),{regs[3]->getInputPin("D")});
    b.addNewWire("carry",fa->getOutputPin("Carry_out"),{regs[4]->getInputPin("D")});
    // Attach sinks so FF outputs use scheduled wire values, not the immediate
    // output-pin cache that the UI can expose before propagation.
    auto sum=b.addNewWire("sum_state",regs[3]->getOutputPin("Q"),{});
    auto carry=b.addNewWire("carry_state",regs[4]->getOutputPin("Q"),{});
    ClockSpecification spec;spec.clock_pin="ADDER.CLK";
    spec.inputs={{"ADDER.RST",std::nullopt,{},0}};
    for (const auto& n:names)spec.inputs.push_back({"ADDER."+n,ClockEdge::Falling,{0,0},0});
    auto report=CircuitTimingAnalyzer::safeClock(root,spec);
    check(report.status==ClockTimingStatus::SafeUnderContracts,"Structural FullAdder must be analyzable");
    const auto period=*report.safe_period_ticks;
    Simulator sim;sim.setHistoryRecordingEnabled(false);
    auto clk=b.addNewWire("clock_drive",nullptr,{root->getInputPin("CLK")});
    auto rst=b.addNewWire("reset_drive",nullptr,{root->getInputPin("RST")});
    std::vector<std::shared_ptr<Wire<>>> data;
    for(const auto& n:names)data.push_back(b.addNewWire("drive"+n,nullptr,{root->getInputPin(n)}));
    auto drive=[&](const auto& wire,size_t t,bool v){sim.scheduleEvent(std::make_shared<WireUpdateEvent<>>(t,wire,v?LogicValue::HIGH:LogicValue::LOW));};
    one->scheduleInitialEvents(sim,0);drive(clk,0,false);drive(rst,0,true);
    for(const auto& w:data)drive(w,0,false);
    sim.runAndRecord(4*period);drive(rst,4*period+1,false);sim.runAndRecord(8*period);
    size_t base=10*period;
    for(unsigned pattern=0;pattern<8;++pattern) {
        for(size_t i=0;i<3;++i)drive(data[i],base-period/2,(pattern>>i)&1);
        drive(clk,base,true);drive(clk,base+period/2,false);
        drive(clk,base+period,true);drive(clk,base+period+period/2,false);
        sim.runAndRecord(base+2*period-1);
        const unsigned expected=((pattern&1)!=0)+((pattern&2)!=0)+((pattern&4)!=0);
        check(sum->getValue()==(expected&1)&&carry->getValue()==(expected>>1),"FullAdder bounded-clock output mismatch");
        base+=3*period;
    }
}
} // namespace
int main() {
    try {
        boundsAndFailures(); cyclesAndClocks(); externalOutputsAndRewire(); builtinStorage(); structuralAdder();
        std::cout<<"Safe clock: full/half-cycle setup, hold, pulse, fail-closed, macros and structural adder passed\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
