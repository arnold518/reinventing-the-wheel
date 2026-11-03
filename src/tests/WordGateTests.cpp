#include "tests/WordGateTests.hpp"
#include "tests/SettledSweep.hpp"
#include "tests/TruthTableComponentTest.hpp"
#include "tests/TestRegistry.hpp"
#include "modules/basic/WordGate.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "simulator/SimulationTest.hpp"
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>

namespace {
using namespace circuit::test;
using circuit::word::GateKind;
constexpr auto L = LogicValue::LOW, H = LogicValue::HIGH, X = LogicValue::UNKNOWN, Z = LogicValue::HIGH_Z;
constexpr std::array<LogicValue, 4> states{L, H, X, Z};
constexpr std::array<GateKind, 6> kinds{GateKind::And, GateKind::Nand, GateKind::Or, GateKind::Nor, GateKind::Xor, GateKind::Not};

// Independent one-bit rules, written out here: a controlling LOW (AND) or HIGH
// (OR) decides the output; any other unknown or floating input gives X.
LogicValue invert(LogicValue v) { return v == H ? L : v == L ? H : X; }
LogicValue rule(GateKind kind, LogicValue a, LogicValue b) {
    const bool known = (a == L || a == H) && (b == L || b == H);
    switch (kind) {
    case GateKind::And: return a == L || b == L ? L : known ? H : X;
    case GateKind::Nand: return invert(rule(GateKind::And, a, b));
    case GateKind::Or: return a == H || b == H ? H : known ? L : X;
    case GateKind::Nor: return invert(rule(GateKind::Or, a, b));
    case GateKind::Xor: return known ? (a == b ? L : H) : X;
    case GateKind::Not: return invert(a);
    }
    throw std::invalid_argument("kind");
}
NamedValues reference(GateKind kind, const NamedValues& in) {
    const bool unary = kind == GateKind::Not;
    const auto& a = in.at(unary ? "IN" : "A");
    LogicVector out(a.size());
    for (size_t i = 0; i < a.size(); ++i) out[i] = rule(kind, a[i], unary ? L : in.at("B")[i]);
    return {{"OUT", out}};
}

size_t sweepValues(GateKind kind, size_t w) {
    const auto& family = circuit::word::gate(kind, w);
    FidelitySweep sweep(family);
    const bool unary = kind == GateKind::Not;
    const uint64_t mask = (uint64_t{1} << w) - 1;
    auto binary = [&](uint64_t a, uint64_t b) {
        return unary ? sweep.gates().binary({{"IN", a}}) : sweep.gates().binary({{"A", a}, {"B", b}});
    };
    auto check = [&](const NamedValues& inputs) { const auto expected = reference(kind, inputs); sweep.evaluate(inputs, &expected); };
    if (w <= 4) {
        const uint64_t count = uint64_t{1} << w;
        for (uint64_t a = 0; a < count; ++a) for (uint64_t b = 0; b < (unary ? 1u : count); ++b) check(binary(a, b));
    } else {
        const uint64_t alternate = 0x5555555555555555ULL & mask;
        for (uint64_t a : {uint64_t{0}, mask, alternate, alternate ^ mask})
            for (uint64_t b : {uint64_t{0}, mask, alternate, alternate ^ mask}) check(binary(a, b));
    }
    std::mt19937_64 rng(0x6a7e0000 + w * 8 + static_cast<unsigned>(kind));
    for (size_t i = 0; i < 256; ++i) {
        const uint64_t a = rng() & mask;
        const uint64_t b = rng() & mask;
        check(binary(a, b));
    }
    // Dense four-state words: every bit independently L, H, X or Z.
    for (size_t i = 0; i < 256; ++i) {
        auto inputs = binary(0, 0);
        for (auto& [name, bits] : inputs) for (auto& bit : bits) bit = states[rng() % 4];
        check(inputs);
    }
    return sweep.vectors();
}

// One root driven by input wires; OUT is sampled every tick.
class Waveform {
public:
    Waveform(const circuit::ComponentFamily& family, circuit::BuildProfile profile)
        : root_(std::dynamic_pointer_cast<IOComponent>(circuit::builtinComponentCatalog().createRoot(
              family.request("DUT"), std::move(profile)).root)) {
        ComponentBuilder builder(root_);
        sim_.setHistoryRecordingEnabled(false);
        for (const auto& [name, pin] : root_->getAllInputPins())
            inputs_[pin->getName()] = builder.addNewWireDynamic("IN_" + pin->getName(), pin->getWidth(), nullptr, {pin});
        // Sample the wire OUT drives: a basic part sets its pin at evaluation
        // and delivers the value through the wire after its delay.
        output_ = builder.addNewWireDynamic("OUT_OUT", root_->getOutputPinDynamic("OUT")->getWidth(), root_->getOutputPinDynamic("OUT"), {});
        SimulationTest::scheduleInitialEventsForTree(root_, sim_, 0);
    }
    void drive(SimulationTime at, const std::string& pin, uint64_t value) {
        sim_.scheduleEvent(makeTestWireUpdate(at, inputs_.at(pin), TestValue(logicBits(inputs_.at(pin)->getWidth(), value))));
    }
    std::vector<std::string> sample(SimulationTime from, SimulationTime to) {
        std::vector<std::string> trace;
        for (SimulationTime t = from; t <= to; ++t) {
            sim_.advanceAndRecord(t);
            trace.push_back(formatLogicVector(output_->getValueVector()));
        }
        return trace;
    }
private:
    std::shared_ptr<IOComponent> root_;
    Simulator sim_;
    std::map<std::string, std::shared_ptr<WireBase>> inputs_;
    std::shared_ptr<WireBase> output_;
};

// Bits change one tick apart, inside one gate delay: the compact form must
// publish each bit when its own gate would.
void compareTiming(GateKind kind, size_t w) {
    const auto& family = circuit::word::gate(kind, w);
    Waveform gates(family, circuit::strictAllStructural());
    Waveform compact(family, circuit::withExactFidelity(circuit::canonicalDefaultProfile(), "DUT", circuit::Fidelity::Behavioral));
    const bool unary = kind == GateKind::Not;
    const std::string pin = unary ? "IN" : "A";
    const SimulationTime settle = 1000, span = 400;
    // Each bit changes once per step, so no pulse is narrower than a gate
    // delay. (Inertial one-bit gates swallow such pulses; compact parts use
    // transport delay, a known limit of every compact part.)
    std::vector<std::pair<SimulationTime, uint64_t>> script{{1, 0}, {settle, 0x1}, {settle + 1, 0x9},
        {settle + 2, 0x89}, {settle + span, 0xff}, {settle + span + 3, 0xf7}};
    // B passes A through: all ones for AND/NAND/XOR, all zeros for OR/NOR.
    const uint64_t pass = kind == GateKind::Or || kind == GateKind::Nor ? 0 : 0xff;
    for (auto* rig : {&gates, &compact}) {
        if (!unary) rig->drive(1, "B", pass);
        for (const auto& [at, value] : script) rig->drive(at, pin, value);
    }
    const auto expected = gates.sample(settle - 1, settle + 2 * span);
    const auto actual = compact.sample(settle - 1, settle + 2 * span);
    for (size_t t = 0; t < expected.size(); ++t)
        if (expected[t] != actual[t])
            throw std::runtime_error(std::string(family.typeName()) + " timing differs at tick " +
                std::to_string(settle - 1 + t) + " in profile " + circuit::timing::activeTimingProfile().name +
                ": gates " + expected[t] + " compact " + actual[t]);
}
} // namespace

void WordGateSweepTest::verifyResults() {
    size_t vectors = 0;
    for (size_t w = 2; w <= 32; ++w) for (auto kind : kinds) vectors += sweepValues(kind, w);
    std::cout << "186 word gates (2 to 32 bits): " << vectors << " vectors against the one-bit rules; gate-level and compact forms agree\n";
    const auto original = circuit::timing::activeTimingProfile().name;
    for (const auto& profile : circuit::timing::timingProfileNames()) {
        circuit::timing::setActiveTimingProfile(profile);
        for (auto kind : kinds) compareTiming(kind, 8);
    }
    circuit::timing::setActiveTimingProfile(original);
    std::cout << "Staggered per-bit changes publish at the gates' times in every timing profile\n";
}

namespace {
struct AndWord8Target {
    static const circuit::ComponentFamily& family() { return circuit::word::gate(GateKind::And, 8); }
};
std::vector<TestRow> andWord8Rows() {
    std::vector<TestRow> rows;
    const auto word = [](uint64_t value) { return logicBits(8, value); };
    const auto add = [&](const LogicVector& a, const LogicVector& b) {
        LogicVector out(8);
        for (size_t i = 0; i < 8; ++i) out[i] = rule(GateKind::And, a[i], b[i]);
        rows.push_back({{{"A", TestValue(a)}, {"B", TestValue(b)}}, {{"OUT", TestValue(out)}}});
    };
    for (auto [a, b] : std::array<std::pair<uint64_t, uint64_t>, 6>{{{0x00, 0xff}, {0xff, 0xff}, {0xaa, 0xff},
                                                                      {0xaa, 0x55}, {0xf0, 0x3c}, {0xff, 0x00}}})
        add(word(a), word(b));
    // Four-state words: X and Z meet a 0 (decided) and a 1 (unknown).
    auto a = word(0x0f), b = word(0x3c);
    a[1] = X; a[2] = Z; a[5] = X; a[6] = Z;
    add(a, b);
    add(b, a);
    return rows;
}
class ANDGateWidth8Test final : public TruthTableComponentTest<AndWord8Target> {
public:
    ANDGateWidth8Test() : TruthTableComponentTest<AndWord8Target>("ANDGateWidth8Test", "AND_GATE_W8_ROOT", andWord8Rows()) {}
};
} // namespace

void appendWordGateScenarios(std::vector<TestRegistryEntry>& entries) {
    entries.push_back({"ANDGateWidth8Test", "WordGateSweepTest", RegisteredTestKind::TruthTable, {"logic.and.8"}, "rows",
        {"simulation", "contract", "truth-table"}, [] { return std::make_unique<ANDGateWidth8Test>(); }, true});
}
