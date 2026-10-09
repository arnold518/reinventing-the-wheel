#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "modules/composite/Adder32.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "simulator/Event.hpp"
#include "simulator/SimulationTest.hpp"
#include "simulator/Simulator.hpp"
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <stdexcept>

using namespace circuit::analysis;
namespace {
using Bits = std::vector<LogicValue>;

void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
const SettlingEndpoint& endpoint(const SettlingReport& report, const std::string& name) {
    for (const auto& e : report.endpoints) if (e.name == name) return e;
    throw std::runtime_error("No endpoint " + name);
}
SettlingReport valid(const SettlingReport& report) {
    for (const auto& d : report.diagnostics) std::cerr << d << '\n';
    check(report.valid, "Expected a valid settling report");
    return report;
}
void invalid(const SettlingReport& report, const std::string& text) {
    bool mentioned = false;
    for (const auto& d : report.diagnostics) mentioned |= d.find(text) != std::string::npos;
    if (report.valid || !mentioned) for (const auto& d : report.diagnostics) std::cerr << d << '\n';
    check(!report.valid && report.endpoints.empty() && mentioned, "Expected rejection mentioning: " + text);
}

// Drives a part through its pins and reports, per output pin, the last tick
// after an input change at which that output still changed.
struct Bench {
    std::shared_ptr<IOComponent> dut;
    std::shared_ptr<IOComponent> root = Component::create<IOComponent>("BENCH");
    std::map<std::string, std::shared_ptr<WireBase>> inputs, outputs;
    Simulator sim;
    size_t now = 0;

    explicit Bench(std::shared_ptr<IOComponent> part) : dut(std::move(part)) {
        root->addChild(dut);
        ComponentBuilder builder(root);
        for (const auto& [name, pin] : dut->getAllInputPins())
            inputs[name] = builder.addNewWireDynamic("i_" + name, pin->getWidth(), nullptr, {pin});
        for (const auto& [name, pin] : dut->getAllOutputPins())
            outputs[name] = builder.addNewWireDynamic("o_" + name, pin->getWidth(), pin, {});
        sim.setHistoryRecordingEnabled(false);
        SimulationTest::scheduleInitialEventsForTree(root, sim, 0);
    }
    void drive(const std::map<std::string, Bits>& vector, size_t time) {
        for (const auto& [name, bits] : vector)
            sim.scheduleEvent(makeWireUpdateEvent(time, inputs.at(name), bits));
    }
    // `gap`: ticks the previous vector is given before the new one arrives.
    std::map<std::string, size_t> change(const std::map<std::string, Bits>& before,
                                         const std::map<std::string, Bits>& after,
                                         size_t gap, size_t watch) {
        now += 1;
        drive(before, now);
        now += gap;
        sim.advanceAndRecord(now - 1);
        std::map<std::string, Bits> seen;
        for (const auto& [name, wire] : outputs) seen[name] = wire->getValueVector();
        drive(after, now);
        std::map<std::string, size_t> last;
        for (size_t tick = 0; tick <= watch; ++tick) {
            sim.advanceAndRecord(now + tick);
            for (const auto& [name, wire] : outputs) {
                auto current = wire->getValueVector();
                if (current != seen[name]) { last[name] = tick; seen[name] = std::move(current); }
            }
        }
        check(!sim.hasPendingEvents(), dut->getName() + ": did not go quiet in the watched window");
        now += watch;
        return last;
    }
};

std::map<std::string, Bits> randomVector(const IOComponent& part, std::mt19937_64& random, bool four_state,
                                         const std::map<std::string, uint64_t>& fixed = {}) {
    std::map<std::string, Bits> vector;
    for (const auto& [name, pin] : part.getAllInputPins()) {
        Bits bits(pin->getWidth());
        const auto held = fixed.find(name);
        for (size_t bit = 0; bit < bits.size(); ++bit)
            bits[bit] = held != fixed.end() ? LogicValue((held->second >> bit) & 1)
                : LogicValue(random() % (four_state ? 4 : 2));
        vector[name] = std::move(bits);
    }
    return vector;
}

// The simulator must never show a change later than the proven bound.
size_t confirm(Bench& bench, const SettlingReport& report, bool four_state, size_t rounds,
               std::mt19937_64& random, const std::map<std::string, uint64_t>& fixed = {}) {
    size_t worst = 0;
    const size_t watch = report.structural + 4;
    auto check_round = [&](const std::map<std::string, Bits>& before, const std::map<std::string, Bits>& after,
                           size_t gap) {
        for (const auto& [name, tick] : bench.change(before, after, gap, watch)) {
            const auto& e = endpoint(report, name);
            check(e.changes, bench.dut->getName() + "." + name + ": changed although proven constant");
            check(tick <= e.bound, bench.dut->getName() + "." + name + ": changed at tick " + std::to_string(tick)
                + ", after the proven bound " + std::to_string(e.bound));
            worst = std::max(worst, tick);
        }
    };
    for (size_t round = 0; round < rounds; ++round) {
        const auto before = randomVector(*bench.dut, random, four_state, fixed);
        const auto after = randomVector(*bench.dut, random, four_state, fixed);
        // A settled start, then a start that is still switching when inputs move.
        check_round(before, after, watch + 1);
        check_round(randomVector(*bench.dut, random, four_state, fixed), before, 1 + random() % 3);
        // Complement of the new vector: every binary input flips at once.
        auto flipped = after;
        for (auto& [name, bits] : flipped) if (!fixed.contains(name)) for (auto& bit : bits)
            bit = bit == LogicValue::LOW ? LogicValue::HIGH : bit == LogicValue::HIGH ? LogicValue::LOW : bit;
        check_round(flipped, after, watch + 1);
    }
    // The vectors the analysis itself names as limiting.
    for (const auto& e : report.endpoints) {
        if (e.witness.empty()) continue;
        auto target = randomVector(*bench.dut, random, false, fixed);
        for (const auto& [name, pin] : bench.dut->getAllInputPins()) {
            const auto it = e.witness.find(pin->getID());
            if (it == e.witness.end()) continue;
            for (size_t bit = 0; bit < pin->getWidth(); ++bit) target[name][bit] = LogicValue((it->second >> bit) & 1);
        }
        for (size_t round = 0; round < 6; ++round) {
            auto before = randomVector(*bench.dut, random, four_state, fixed);
            if (round == 0) {
                before = target;
                for (auto& [name, bits] : before) if (!fixed.contains(name)) for (auto& bit : bits)
                    bit = bit == LogicValue::LOW ? LogicValue::HIGH : LogicValue::LOW;
            }
            check_round(before, target, watch + 1);
        }
    }
    return worst;
}

struct Part {
    std::shared_ptr<IOComponent> root;
    ComponentBuilder b;
    explicit Part(const std::string& name, IOComponent::PinInitFunction pins)
        : root(Component::create<IOComponent>(name, std::move(pins))), b(root) {}
    std::shared_ptr<Pin<>> chain(std::shared_ptr<Pin<>> from, const std::string& prefix, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            auto gate = b.addNewComponent<NOTGate>(prefix + std::to_string(i));
            b.addNewWire(prefix + "_w" + std::to_string(i), from, {gate->getInputPin("IN")});
            from = gate->getOutputPin("OUT");
        }
        return from;
    }
    template<class Gate> std::shared_ptr<Pin<>> gate(const std::string& name, std::shared_ptr<Pin<>> a,
                                                     std::shared_ptr<Pin<>> b_pin) {
        auto g = b.addNewComponent<Gate>(name);
        nets[a].push_back(g->getInputPin("A"));
        nets[b_pin].push_back(g->getInputPin("B"));
        return g->getOutputPin("OUT");
    }
    std::shared_ptr<Pin<>> invert(const std::string& name, std::shared_ptr<Pin<>> a) {
        auto g = b.addNewComponent<NOTGate>(name);
        nets[a].push_back(g->getInputPin("IN"));
        return g->getOutputPin("OUT");
    }
    void out(std::shared_ptr<Pin<>> from, const std::string& pin) { nets[from].push_back(root->getOutputPin(pin)); }
    void finish() {
        unsigned n = 0;
        for (auto& [source, sinks] : nets) b.addNewWire("net" + std::to_string(n++), source, sinks);
    }
    std::map<std::shared_ptr<Pin<>>, std::vector<std::shared_ptr<Pin<>>>> nets;
};

// Two selectors on the same select: the long input of the first can only
// reach the output when S=1 there and S=0 at the second. No binary S does both.
std::shared_ptr<IOComponent> falsePathPart() {
    Part p("FALSE_PATH", [](IOComponent* c) {
        for (auto name : {"A", "B", "C", "S"}) c->addPin(name, PinType::INPUT);
        c->addPin("Z", PinType::OUTPUT);
    });
    auto in = [&](const char* name) { return p.root->getInputPin(name); };
    auto slow = in("A");
    for (unsigned i = 0; i < 6; ++i) slow = p.invert("SLOW" + std::to_string(i), slow);
    const auto not_s = p.invert("NOT_S", in("S"));
    const auto y = p.gate<ORGate>("Y", p.gate<ANDGate>("Y1", slow, in("S")), p.gate<ANDGate>("Y0", in("B"), not_s));
    p.out(p.gate<ORGate>("Z", p.gate<ANDGate>("Z1", in("C"), in("S")), p.gate<ANDGate>("Z0", y, not_s)), "Z");
    p.finish();
    return p.root;
}

void handBuilt() {
    std::mt19937_64 random(0x5e771e);
    {
        Part p("CHAIN", [](IOComponent* c) { c->addPin("A", PinType::INPUT); c->addPin("Y", PinType::OUTPUT); });
        p.out(p.chain(p.root->getInputPin("A"), "N", 3), "Y");
        p.finish();
        const auto r = valid(CircuitTimingAnalyzer::settling(p.root));
        check(r.bound == 3 && r.structural == 3, "Three inverters take three ticks");
        Bench bench(p.root);
        check(confirm(bench, r, true, 8, random) == 3, "The inverter chain bound is attained");
    }
    {
        const auto part = falsePathPart();
        const auto all = valid(CircuitTimingAnalyzer::settling(part));
        check(endpoint(all, "Z").structural == 10 && endpoint(all, "Z").bound == 10,
              "With an X select both selectors stay live: 10 ticks");
        SettlingOptions binary;
        binary.binary_sources = true;
        const auto bin = valid(CircuitTimingAnalyzer::settling(part, binary));
        check(endpoint(bin, "Z").bound == 5 && endpoint(bin, "Z").tight, "Binary select removes the false path: 5 ticks");
        Bench four(part), two(falsePathPart());
        check(confirm(four, all, true, 400, random) > 5, "A four-state select must be seen to use the long path");
        check(confirm(two, valid(CircuitTimingAnalyzer::settling(two.dut, binary)), false, 400, random) == 5,
              "The binary bound of 5 is attained and never exceeded");

        SettlingOptions held;
        held.fixed_inputs["S"] = 1;
        const auto fixed = valid(CircuitTimingAnalyzer::settling(part, held));
        check(endpoint(fixed, "Z").bound == 2, "S held at 1 leaves only C -> Z");
        Bench hold(falsePathPart());
        check(confirm(hold, valid(CircuitTimingAnalyzer::settling(hold.dut, held)), true, 200, random, held.fixed_inputs) == 2,
              "Held select bound is attained");
    }
    {
        // AND with a constant 0 never changes; OR with it is a plain buffer.
        Part p("CONSTANT", [](IOComponent* c) {
            c->addPin("A", PinType::INPUT); c->addPin("DEAD", PinType::OUTPUT); c->addPin("LIVE", PinType::OUTPUT);
        });
        auto zero = p.b.addNewComponent<ConstantValue<1>>("ZERO", 0);
        const auto slow = p.chain(p.root->getInputPin("A"), "N", 4);
        p.out(p.gate<ANDGate>("AND", slow, zero->getOutputPin("OUT")), "DEAD");
        p.out(p.gate<ORGate>("OR", slow, zero->getOutputPin("OUT")), "LIVE");
        p.finish();
        const auto r = valid(CircuitTimingAnalyzer::settling(p.root));
        check(!endpoint(r, "DEAD").changes && endpoint(r, "LIVE").bound == 5, "Constant folding");
        Bench bench(p.root);
        confirm(bench, r, true, 40, random);
    }
    {
        // A bus that is re-joined after one inverter per bit. Taken as whole
        // pins, JOIN -> SPLIT -> NOT -> JOIN would be a loop.
        auto root = Component::create<IOComponent>("PER_BIT", [](IOComponent* c) {
            c->addPin("A", PinType::INPUT); c->addPin<4>("Y", PinType::OUTPUT);
        });
        ComponentBuilder b(root);
        auto join = b.addNewComponent<BitJoiner<4>>("JOIN");
        auto split = b.addNewComponent<BitSplitter<4>>("SPLIT");
        b.addNewWire<4>("bus", join->getOutputPin<4>("OUT"), {split->getInputPin<4>("IN"), root->getOutputPin<4>("Y")});
        b.addNewWire("a", root->getInputPin("A"), {join->getInputPin("IN_0")});
        for (unsigned bit = 0; bit < 3; ++bit) {
            auto gate = b.addNewComponent<NOTGate>("N" + std::to_string(bit));
            b.addNewWire("s" + std::to_string(bit), split->getOutputPin("OUT_" + std::to_string(bit)), {gate->getInputPin("IN")});
            b.addNewWire("n" + std::to_string(bit), gate->getOutputPin("OUT"), {join->getInputPin("IN_" + std::to_string(bit + 1))});
        }
        auto unused = b.addNewComponent<NOTGate>("UNUSED");
        b.addNewWire("s3", split->getOutputPin("OUT_3"), {unused->getInputPin("IN")});
        const auto r = valid(CircuitTimingAnalyzer::settling(root));
        check(endpoint(r, "Y").bound == 6, "Per-bit: three stages of splitter + inverter");
        Bench bench(root);
        check(confirm(bench, r, true, 20, random) == 6, "Per-bit bound is attained");
    }
}

class TestRegister : public BasicComponent {
public:
    explicit TestRegister(std::string name) : BasicComponent(std::move(name), 2, [](IOComponent* c) {
        c->addPin("CLK", PinType::INPUT); c->addPin("D", PinType::INPUT); c->addPin("Q", PinType::OUTPUT);
    }) {}
    void evaluate(size_t, Simulator&) override {}
};
TimingContracts registerContract() {
    return {[](const IOComponent& c) -> std::optional<ClockComponentContract> {
        if (!dynamic_cast<const TestRegister*>(&c)) return std::nullopt;
        ClockComponentContract m;
        m.evidence = "test register";
        m.clock_pin = "CLK";
        m.captures = {{"D", ClockEdge::Rising, 1, 0}};
        m.launches = {{"Q", ClockEdge::Rising, {2, 2}}};
        return m;
    }};
}

void boundaries() {
    auto build = [](bool gated_clock) {
        Part p("STATE", [](IOComponent* c) {
            for (auto name : {"CLK", "IN", "EN"}) c->addPin(name, PinType::INPUT);
            c->addPin("OUT", PinType::OUTPUT);
        });
        auto launch = p.b.addNewComponent<TestRegister>("L"), capture = p.b.addNewComponent<TestRegister>("C");
        auto merge = p.b.addNewComponent<ANDGate>("MERGE");
        const auto clock = p.root->getInputPin("CLK"), enable = p.root->getInputPin("EN");
        if (gated_clock) {
            auto gate = p.b.addNewComponent<ANDGate>("GATE");
            p.b.addNewWire("clk", clock, {gate->getInputPin("A"), launch->getInputPin("CLK")});
            p.b.addNewWire("en", enable, {gate->getInputPin("B"), merge->getInputPin("B")});
            p.b.addNewWire("gclk", gate->getOutputPin("OUT"), {capture->getInputPin("CLK")});
        } else {
            p.b.addNewWire("clk", clock, {launch->getInputPin("CLK"), capture->getInputPin("CLK")});
            p.b.addNewWire("not_en", p.chain(enable, "E", 1), {merge->getInputPin("B")});
        }
        p.b.addNewWire("in", p.root->getInputPin("IN"), {launch->getInputPin("D")});
        p.b.addNewWire("q3", p.chain(launch->getOutputPin("Q"), "N", 3), {merge->getInputPin("A"), p.root->getOutputPin("OUT")});
        p.b.addNewWire("d", merge->getOutputPin("OUT"), {capture->getInputPin("D")});
        return p.root;
    };
    SettlingOptions options;
    options.contracts = registerContract();
    const auto part = build(false);
    auto r = valid(CircuitTimingAnalyzer::settling(part, options));
    // Launch 2, three inverters, AND, setup 1.
    check(endpoint(r, "(captures)").bound == 7 && endpoint(r, "OUT").bound == 5, "Clocked launch to capture");
    options.sources = SettlingSources::InputsOnly;
    r = valid(CircuitTimingAnalyzer::settling(part, options));
    // EN -> inverter -> AND -> setup; IN reaches L.D directly (setup only).
    check(endpoint(r, "(captures)").bound == 3 && !endpoint(r, "OUT").changes, "Inputs only: state holds");
    options.sources = SettlingSources::StateOnly;
    r = valid(CircuitTimingAnalyzer::settling(part, options));
    check(endpoint(r, "(captures)").bound == 7 && endpoint(r, "OUT").bound == 5, "State only");

    // Both questions at once: what a compact form of this part must reproduce.
    options.sources = SettlingSources::All;
    const auto timing = CircuitTimingAnalyzer::characterize(part, options);
    check(timing.valid && timing.clocked && timing.delay == 5, "Characterized part");
    check(timing.input_to_output.size() == 1 && !timing.input_to_output[0].changes && timing.input_to_capture == 3,
          "Inputs reach only the capture");
    check(timing.clock_to_output.size() == 1 && timing.clock_to_output[0].delay == 5 && timing.state_to_capture == 7,
          "State reaches the output and the capture");

    invalid(CircuitTimingAnalyzer::settling(part), "no timing contract");
    options.sources = SettlingSources::All;
    invalid(CircuitTimingAnalyzer::settling(build(true), options), "derived, gated or undriven clock");
    options.fixed_inputs["MISSING"] = 0;
    invalid(CircuitTimingAnalyzer::settling(part, options), "names no root input");

    Part latch("LATCH", [](IOComponent* c) {
        c->addPin("S", PinType::INPUT); c->addPin("R", PinType::INPUT); c->addPin("Q", PinType::OUTPUT);
    });
    auto a = latch.b.addNewComponent<NORGate>("A"), b = latch.b.addNewComponent<NORGate>("B");
    latch.b.addNewWire("s", latch.root->getInputPin("S"), {b->getInputPin("A")});
    latch.b.addNewWire("r", latch.root->getInputPin("R"), {a->getInputPin("A")});
    latch.b.addNewWire("q", a->getOutputPin("OUT"), {b->getInputPin("B"), latch.root->getOutputPin("Q")});
    latch.b.addNewWire("qn", b->getOutputPin("OUT"), {a->getInputPin("B")});
    invalid(CircuitTimingAnalyzer::settling(latch.root), "cycle");
}

// Every gate-level part in the catalog: the three bounds must be ordered and
// the simulator must respect them.
void catalog(size_t rounds, size_t budget, bool verbose, const std::set<std::string>& only) {
    const auto& catalog = circuit::builtinComponentCatalog();
    std::mt19937_64 random(0x20261009);
    size_t confirmed = 0, lowered = 0, enumerated = 0, compared = 0, differing = 0, tabled = 0;
    std::cout << std::left << std::setw(44) << "contract" << std::right << std::setw(7) << "paths" << std::setw(7)
              << "any" << std::setw(7) << "0/1" << std::setw(7) << "tight" << std::setw(7)
              << "seen" << std::setw(7) << "seen01" << std::setw(9) << "compact" << '\n';
    for (const auto& contract : catalog.contracts()) {
        if (!only.empty() && !only.contains(contract.id)) continue;
        std::shared_ptr<IOComponent> part, twin;
        try {
            part = std::dynamic_pointer_cast<IOComponent>(
                catalog.createRoot({contract.id, "DUT", {}}, circuit::maximallyStructural()).root);
            twin = std::dynamic_pointer_cast<IOComponent>(
                catalog.createRoot({contract.id, "DUT", {}}, circuit::maximallyStructural()).root);
        } catch (const std::exception&) { continue; }
        if (!part || part->getAllOutputPins().empty() || part->getAllInputPins().empty()) continue;
        const auto all = CircuitTimingAnalyzer::settling(part);
        if (!all.valid) continue;
        bool clocked = false;
        for (const auto& e : all.endpoints) clocked |= e.name == "(captures)";
        if (clocked) continue; // clocked parts need an edge-driven bench; not confirmed here
        // A self-timed part also changes at times of its own; its bounds count
        // from whatever caused the change, not from the input change alone.
        if (all.self_timed) continue;
        SettlingOptions options;
        options.binary_sources = true;
        options.search_budget = budget;
        const auto binary = CircuitTimingAnalyzer::settling(part, options);
        check(binary.valid, contract.id + ": binary analysis rejected a part the all-state analysis accepted");
        bool tight = true;
        for (const auto& e : all.endpoints) {
            const auto& b = endpoint(binary, e.name);
            check(b.bound <= e.bound && e.bound <= e.structural, contract.id + "." + e.name + ": bounds out of order");
            tight &= b.tight || !b.changes;
        }
        // Small parts: try every final vector. The search may never report
        // less than the largest single-vector value, and "tight" must equal it.
        size_t input_bits = 0;
        for (const auto& [name, pin] : part->getAllInputPins()) input_bits += pin->getWidth();
        if (input_bits <= 12) {
            std::map<std::string, size_t> largest;
            for (uint64_t vector = 0; vector < (uint64_t{1} << input_bits); ++vector) {
                SettlingOptions one = options;
                size_t offset = 0;
                for (const auto& [name, pin] : part->getAllInputPins()) {
                    one.final_inputs[name] = (vector >> offset) & ((uint64_t{1} << pin->getWidth()) - 1);
                    offset += pin->getWidth();
                }
                for (const auto& e : valid(CircuitTimingAnalyzer::settling(part, one)).endpoints)
                    largest[e.name] = std::max(largest[e.name], e.bound);
            }
            // Both ways of choosing cases, each on its own.
            for (const auto search : {SettlingSearch::Variables, SettlingSearch::Signals}) {
                SettlingOptions single = options;
                single.search = search;
                for (const auto& e : valid(CircuitTimingAnalyzer::settling(part, single)).endpoints) {
                    check(e.bound >= largest[e.name], contract.id + "." + e.name + ": search went below a real case");
                    check(!e.tight || e.bound == largest[e.name], contract.id + "." + e.name + ": tight but not attained");
                    check(!e.changes || e.tight, contract.id + "." + e.name + ": a small part was left undecided");
                }
            }
            ++enumerated;
        }
        if (verbose) for (const auto& e : binary.endpoints)
            std::cout << "  " << e.name << ": paths " << e.structural << ", any " << endpoint(all, e.name).bound
                      << ", 0/1 " << e.bound << (e.tight ? " tight" : "") << ", " << e.evaluations << " evaluations\n";
        if (verbose) for (const auto& e : binary.endpoints) if (e.changes && !e.tight) {
            std::cout << "  open path to " << e.name << ":\n";
            for (const auto& step : e.path) if (step.find(".output.") != std::string::npos || step.find(".input.") == std::string::npos)
                std::cout << "    " << step << '\n';
        }
        // The whole-part delay, next to the delay its compact form uses today.
        const auto timing = CircuitTimingAnalyzer::characterize(part, options);
        check(timing.valid && !timing.clocked && timing.delay == binary.bound, contract.id + ": characterization disagrees");
        std::string compact = "-";
        try {
            const auto behavioral = catalog.createRoot({contract.id, "DUT", {}}, circuit::strictAllBehavioral()).root;
            if (const auto leaf = std::dynamic_pointer_cast<BasicComponent>(behavioral)) {
                ++compared;
                if (!leaf->getDelays()) {
                    compact = std::to_string(leaf->getDelay());
                    differing += leaf->getDelay() != timing.delay;
                } else {
                    // The compact part runs on its proven table: side by side
                    // with the gates it gives the same values, is never
                    // earlier than they are, and never later than proven.
                    compact = "table";
                    ++tabled;
                    Bench gates(std::dynamic_pointer_cast<IOComponent>(
                        catalog.createRoot({contract.id, "DUT", {}}, circuit::maximallyStructural()).root));
                    Bench modeled(std::dynamic_pointer_cast<IOComponent>(behavioral));
                    const auto declared = valid(CircuitTimingAnalyzer::settling(modeled.dut, options));
                    const size_t watch = all.structural + 4;
                    for (size_t round = 0; round < 4 * rounds; ++round) {
                        const auto before = randomVector(*gates.dut, random, false);
                        auto after = randomVector(*gates.dut, random, false);
                        if (round % 2) { // only one input pin changes
                            auto pin = after.begin();
                            std::advance(pin, random() % after.size());
                            const auto kept = *pin;
                            after = before;
                            after[kept.first] = kept.second;
                        }
                        const auto real = gates.change(before, after, watch + 1, watch);
                        const auto shown = modeled.change(before, after, watch + 1, watch);
                        for (const auto& [name, wire] : gates.outputs)
                            check(wire->getValueVector() == modeled.outputs.at(name)->getValueVector(),
                                  contract.id + "." + name + ": the compact part's value differs from the gates");
                        for (const auto& [name, tick] : shown) {
                            check(tick <= endpoint(declared, name).bound, contract.id + "." + name + ": the compact part changed after its table allows");
                            const auto it = real.find(name);
                            check(it == real.end() || tick >= it->second, contract.id + "." + name + ": the compact part changed before the gates finished");
                        }
                    }
                    for (const auto& e : binary.endpoints)
                        check(endpoint(declared, e.name).bound >= e.bound || !e.changes,
                              contract.id + "." + e.name + ": the declared table is faster than the gates");
                }
            }
        } catch (const std::exception&) {}
        Bench four(part), two(twin);
        const auto seen = confirm(four, all, true, rounds, random);
        const auto seen_binary = confirm(two, valid(CircuitTimingAnalyzer::settling(twin, options)), false, rounds, random);
        ++confirmed;
        lowered += binary.bound < all.structural;
        std::cout << std::left << std::setw(44) << contract.id << std::right << std::setw(7) << all.structural
                  << std::setw(7) << all.bound << std::setw(7) << binary.bound << std::setw(7) << (tight ? "yes" : "no")
                  << std::setw(7) << seen << std::setw(7) << seen_binary << std::setw(9) << compact << '\n';
    }
    std::cout << confirmed << " gate-level parts confirmed against the simulator; " << lowered
              << " have a proven bound below the longest path; " << enumerated
              << " small parts checked against every final vector\n"
              << compared << " of them have a compact form; " << tabled << " run on a proven delay table and matched their gates; "
              << differing << " others use a single delay different from the proven one\n";
    check(!only.empty() || (confirmed >= 80 && enumerated >= 25 && lowered >= 3), "Too few parts were confirmed");
}
// A false path in a real part. The longest path to the ALU's EQ output runs
// through the subtractor's carry chain into the zero detector. It needs every
// other difference bit to be 0, which makes all bits propagate, and then the
// carry is already fixed by the short carry-in path.
void aluEquality() {
    const auto& catalog = circuit::builtinComponentCatalog();
    const auto build = [&] {
        return std::dynamic_pointer_cast<IOComponent>(
            catalog.createRoot({"rv32i.alu32", "DUT", {}}, circuit::maximallyStructural()).root);
    };
    SettlingOptions options;
    options.binary_sources = true;
    options.search_budget = 500000;
    Bench bench(build());
    const auto report = valid(CircuitTimingAnalyzer::settling(bench.dut, options));
    const auto& eq = endpoint(report, "EQ");
    std::cout << "rv32i.alu32 EQ: paths " << eq.structural << ", proven " << eq.bound
              << (eq.tight ? " (attained)" : "") << " after " << eq.evaluations << " evaluations\n";
    check(eq.structural == 33 && eq.bound == 30 && eq.tight, "The ALU EQ false path must be removed: 33 -> 30");

    std::mt19937_64 random(0xe9);
    const auto word = [](uint32_t value) {
        Bits bits(32);
        for (unsigned bit = 0; bit < 32; ++bit) bits[bit] = LogicValue((value >> bit) & 1);
        return bits;
    };
    const auto vector = [&](uint32_t a, uint32_t b, unsigned op) {
        Bits code(5);
        for (unsigned bit = 0; bit < 5; ++bit) code[bit] = LogicValue((op >> bit) & 1);
        return std::map<std::string, Bits>{{"A", word(a)}, {"B", word(b)}, {"OP", code}};
    };
    // Equal and nearly equal operands under every operation: the vectors
    // that exercise the carry chain into the zero detector.
    size_t worst = 0;
    for (unsigned round = 0; round < 1200; ++round) {
        const uint32_t a = round % 3 == 0 ? 0xffffffffu : static_cast<uint32_t>(random());
        const uint32_t near = round % 4 == 0 ? a : a ^ (uint32_t{1} << (random() % 32));
        const auto before = round % 2 ? vector(static_cast<uint32_t>(random()), static_cast<uint32_t>(random()), random() % 32)
                                      : vector(~a, a, random() % 32);
        for (const auto& [name, tick] : bench.change(before, vector(a, near, random() % 32), 60, 50)) {
            check(tick <= endpoint(report, name).bound, "rv32i.alu32." + name + ": changed at tick "
                + std::to_string(tick) + ", after the proven bound " + std::to_string(endpoint(report, name).bound));
            if (name == "EQ") worst = std::max(worst, tick);
        }
    }
    std::cout << "rv32i.alu32 EQ: latest change seen on equal and near-equal operands: tick " << worst << '\n';
    confirm(bench, report, false, 60, random);
}
// A compact part that carries the delay table proven on its gate-level form.
// Checked on the adder: alone, and as the children of a larger circuit.
std::shared_ptr<IOComponent> twoAdders(bool compact, const circuit::PartDelays* table) {
    const auto& catalog = circuit::builtinComponentCatalog();
    const auto profile = compact ? circuit::strictAllBehavioral() : circuit::maximallyStructural();
    auto manifest = std::make_shared<circuit::BuildManifest>(profile.name(), profile.fingerprint());
    auto context = circuit::BuildContext::rootScope(catalog, std::make_shared<const circuit::BuildProfile>(profile), manifest)
        ->child("PAIR", std::nullopt);
    auto root = Component::createWithContext<IOComponent>(context, "PAIR", [](IOComponent* c) {
        for (auto name : {"A", "B", "C"}) c->addPin<32>(name, PinType::INPUT);
        c->addPin("Cin", PinType::INPUT);
        c->addPin<32>("OUT", PinType::OUTPUT);
    });
    ComponentBuilder b(root, context);
    auto first = b.add(circuit::families::Adder32, "FIRST"), second = b.add(circuit::families::Adder32, "SECOND");
    b.addNewWire<32>("a", root->getInputPin<32>("A"), {first->getInputPin<32>("A")});
    b.addNewWire<32>("b", root->getInputPin<32>("B"), {first->getInputPin<32>("B")});
    b.addNewWire<32>("c", root->getInputPin<32>("C"), {second->getInputPin<32>("B")});
    b.addNewWire("cin", root->getInputPin("Cin"), {first->getInputPin("Cin")});
    b.addNewWire<32>("sum", first->getOutputPin<32>("Sum"), {second->getInputPin<32>("A")});
    b.addNewWire("carry", first->getOutputPin("Cout"), {second->getInputPin("Cin")});
    b.addNewWire<32>("out", second->getOutputPin<32>("Sum"), {root->getOutputPin<32>("OUT")});
    if (table) for (const auto& adder : {first, second}) {
        const auto leaf = std::dynamic_pointer_cast<BasicComponent>(adder);
        check(leaf != nullptr, "The compact adder must be one evaluated part");
        leaf->setDelays(*table);
    }
    return root;
}

void compactAdder() {
    const auto& catalog = circuit::builtinComponentCatalog();
    const auto build = [&](const circuit::BuildProfile& profile) {
        return std::dynamic_pointer_cast<IOComponent>(catalog.createRoot({"arithmetic.adder.width32", "DUT", {}}, profile).root);
    };
    SettlingOptions options;
    options.binary_sources = true;
    Bench gates(build(circuit::maximallyStructural())), compact(build(circuit::strictAllBehavioral()));
    std::vector<std::string> problems;
    const auto table = CircuitTimingAnalyzer::delays(gates.dut, options, "", &problems);
    for (const auto& problem : problems) std::cerr << problem << '\n';
    std::map<std::pair<std::string, std::string>, circuit::DelayRange> arcs;
    for (const auto& arc : table.arcs) arcs[{arc.input, arc.output}] = arc.delay;
    check(arcs.size() == 6 && table.clock.empty() && table.launches.empty(), "The adder has six input-output pairs");
    check(arcs[{"A", "Sum"}].maximum == 19 && arcs[{"Cin", "Sum"}].maximum == 3 && arcs[{"Cin", "Cout"}].maximum == 2,
          "Proven adder pairs");
    for (const auto& [pair, delay] : arcs) check(delay.minimum >= 1 && delay.minimum <= delay.maximum, "Earliest before latest");
    std::cout << "Adder32 pairs (earliest..latest):";
    for (const auto& [pair, delay] : arcs) std::cout << ' ' << pair.first << "->" << pair.second << ' ' << delay.minimum << ".." << delay.maximum;
    std::cout << '\n';

    // The analyzer reads the table as the compact part's contract.
    // The catalog has already given the compact adder its stored table; it
    // must be the one just proven. Without a table the part has no contract.
    const auto leaf = std::dynamic_pointer_cast<BasicComponent>(compact.dut);
    check(leaf && leaf->getDelays() && leaf->getDelays()->arcs.size() == table.arcs.size(), "The compact adder must carry its stored table");
    for (size_t i = 0; i < table.arcs.size(); ++i) {
        const auto& stored = leaf->getDelays()->arcs[i];
        check(stored.input == table.arcs[i].input && stored.output == table.arcs[i].output
              && stored.delay.minimum == table.arcs[i].delay.minimum && stored.delay.maximum == table.arcs[i].delay.maximum,
              "The stored adder table differs from the gates");
    }
    leaf->setDelays({});
    check(!CircuitTimingAnalyzer::settling(compact.dut).valid, "A compact part without a table has no contract");
    leaf->setDelays(table);
    const auto declared = valid(CircuitTimingAnalyzer::settling(compact.dut, options));
    const auto proven = valid(CircuitTimingAnalyzer::settling(gates.dut, options));
    for (const auto& e : proven.endpoints) check(endpoint(declared, e.name).bound == e.bound, "Compact and gate-level bounds differ");

    // In the simulator the compact part gives the same values, never changes
    // before the gates have finished, and never after its table allows.
    std::mt19937_64 random(0xadd);
    size_t carry_only = 0, glitches = 0;
    for (unsigned round = 0; round < 600; ++round) {
        const auto before = randomVector(*gates.dut, random, false);
        auto after = randomVector(*gates.dut, random, false);
        std::string changing;
        if (round % 3 == 1) { after = before; after["Cin"][0] = before.at("Cin")[0] == LogicValue::LOW ? LogicValue::HIGH : LogicValue::LOW; changing = "Cin"; }
        if (round % 3 == 2) { after = before; after["A"] = randomVector(*gates.dut, random, false)["A"]; changing = "A"; }
        const auto real = gates.change(before, after, 30, 30), modeled = compact.change(before, after, 30, 30);
        for (const auto& [name, wire] : gates.outputs)
            check(wire->getValueVector() == compact.outputs.at(name)->getValueVector(), "Compact adder value differs");
        for (const auto& [name, tick] : modeled) {
            const auto limit = changing.empty() ? endpoint(proven, name).bound : arcs[{changing, name}].maximum;
            check(tick <= limit, "Compact adder changed after its table allows");
            if (changing == "Cin") { check(tick <= 3, "A carry-in change must be fast"); ++carry_only; }
        }
        // The gates may glitch and return to the old value; the compact part
        // then shows no change at all. Where it does change, it is not early.
        for (const auto& [name, tick] : real) {
            const auto it = modeled.find(name);
            if (it != modeled.end()) check(it->second >= tick, "Compact adder changed before the gates finished");
            else ++glitches;
        }
    }
    check(carry_only > 50, "Carry-in changes were not exercised");
    std::cout << "Compact Adder32 on its proven table: 600 input changes matched; the gates glitched without a final change "
              << glitches << " times\n";

    // Parents: built from compact adders, a circuit is never faster than the
    // same circuit in gates, in the analysis and in the simulator.
    Bench parent_gates(twoAdders(false, nullptr)), parent_compact(twoAdders(true, &table));
    const auto slow = valid(CircuitTimingAnalyzer::settling(parent_compact.dut, options));
    const auto fast = valid(CircuitTimingAnalyzer::settling(parent_gates.dut, options));
    const auto compact_bound = endpoint(slow, "OUT").bound, gate_bound = endpoint(fast, "OUT").bound;
    std::cout << "Two chained adders: " << gate_bound << " ticks in gates, " << compact_bound << " with compact adders\n";
    check(compact_bound >= gate_bound, "A compact parent may not be faster than its gates");
    for (unsigned round = 0; round < 300; ++round) {
        const auto before = randomVector(*parent_gates.dut, random, false), after = randomVector(*parent_gates.dut, random, false);
        const auto real = parent_gates.change(before, after, 60, 60), modeled = parent_compact.change(before, after, 60, 60);
        check(parent_gates.outputs.at("OUT")->getValueVector() == parent_compact.outputs.at("OUT")->getValueVector(),
              "Compact parent value differs");
        if (modeled.contains("OUT")) check(modeled.at("OUT") <= compact_bound, "Compact parent exceeded its bound");
        if (real.contains("OUT") && modeled.contains("OUT"))
            check(modeled.at("OUT") >= real.at("OUT"), "Compact parent settled before its gates");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        size_t rounds = 12, budget = 4000;
        bool verbose = false;
        std::set<std::string> only;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument.rfind("--rounds=", 0) == 0) rounds = std::stoul(argument.substr(9));
            else if (argument.rfind("--budget=", 0) == 0) budget = std::stoul(argument.substr(9));
            else if (argument == "--verbose") verbose = true;
            else only.insert(argument);
        }
        if (only.empty()) { handBuilt(); boundaries(); compactAdder(); aluEquality(); }
        catalog(rounds, budget, verbose, only);
        std::cout << "SettlingBoundTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SettlingBoundTest failed: " << error.what() << '\n';
        return 1;
    }
}
