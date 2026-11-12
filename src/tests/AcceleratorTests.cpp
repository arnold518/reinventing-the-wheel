// Chapter 6 component checks: the array multiplier, the multiply-add cell, the
// requantizer and the systolic array. Each part is checked on its gates and on
// its compact form against an independent integer reference written here.
#include "tests/AcceleratorTests.hpp"
#include "tests/TestRegistry.hpp"
#include "tests/TestHelpers.hpp"
#include "tests/ComponentTestModel.hpp"
#include "modules/composite/ArrayMultiplier.hpp"
#include "modules/accelerator/MatrixParts.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/BasicComponent.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/selection/BuildProfile.hpp"
#include "simulator/Simulator.hpp"
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <random>

namespace circuit::accelerator::test {

void require(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }

BuildProfile compactProfile() {
    return BuildProfileBuilder("accelerator-compact-children")
        .addRule(preferFidelity(Fidelity::Behavioral, ProfileSelector::any()))
        .unavailablePolicy(UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException).build();
}

Rig::Rig(const ComponentFamily& f, bool compact, ParameterMap parameters) {
    attach(builtinComponentCatalog().createRoot(f.request("DUT", std::move(parameters)),
                                                compact ? compactProfile() : strictAllStructural()).root);
}
Rig::Rig(const std::string& contract, bool compact, ParameterMap parameters) {
    attach(builtinComponentCatalog().createRoot({contract, "DUT", std::move(parameters)},
                                                compact ? compactProfile() : strictAllStructural()).root);
}
void Rig::attach(std::shared_ptr<Component> built) {
    root = std::dynamic_pointer_cast<IOComponent>(built);
    ComponentBuilder b(root);
    for (auto& [n, p] : root->getAllInputPins()) inputs[n] = b.addNewWireDynamic("IN_" + n, p->getWidth(), nullptr, {p});
    for (auto& [n, p] : root->getAllOutputPins()) b.addNewWireDynamic("OUT_" + n, p->getWidth(), p, {});
    sim.setHistoryRecordingEnabled(false);
    SimulationTest::scheduleInitialEventsForTree(root, sim);
    for (auto& [n, p] : inputs)
        sim.scheduleEvent(makeTestWireUpdate(0, p, TestValue(uint64_t(n == "RST" || n.ends_with("_RST")))));
    sim.advanceAndRecord(now = 1000);
    if (inputs.contains("CLK")) cycle();
    std::map<std::string, uint64_t> release;
    for (auto& [n, p] : inputs) if (n == "RST" || n.ends_with("_RST")) release[n] = 0;
    if (!release.empty()) drive(release);
}
void Rig::drive(const std::map<std::string, uint64_t>& values) {
    for (auto& [n, x] : values) sim.scheduleEvent(makeTestWireUpdate(now + 1, inputs.at(n), TestValue(x)));
    sim.advanceAndRecord(now += 1000);
}
void Rig::unknown(const std::string& n) {
    sim.scheduleEvent(makeTestWireUpdate(now + 1, inputs.at(n), TestValue(std::vector<LogicValue>(inputs.at(n)->getWidth(), LogicValue::UNKNOWN))));
    sim.advanceAndRecord(now += 1000);
}
uint64_t Rig::out(const std::string& n) const {
    auto p = root->getOutputPinDynamic(n);
    for (auto x : p->getValueAsVector()) require(x == LogicValue::LOW || x == LogicValue::HIGH, "Unknown output " + n);
    return p->getValueAsUInt64();
}
void Rig::cycle() {
    std::map<std::string, uint64_t> low, high;
    for (auto& [n, p] : inputs) if (n == "CLK" || n.ends_with("_CLK")) { low[n] = 0; high[n] = 1; }
    drive(low); drive(high); drive(low);
}

int64_t signExtend(uint64_t v, size_t w) { return w == 64 ? int64_t(v) : int64_t(v << (64 - w)) >> (64 - w); }

// Independent references.
uint64_t referenceProduct(uint64_t a, uint64_t b, bool sa, bool sb, size_t w) {
    const __int128 x = sa ? __int128(signExtend(a, w)) : __int128(a);
    const __int128 y = sb ? __int128(signExtend(b, w)) : __int128(b);
    const __int128 p = x * y;
    const uint64_t mask = w == 32 ? ~uint64_t{0} : (uint64_t{1} << (2 * w)) - 1;
    return uint64_t(p) & mask;
}
int8_t referenceRequantize(int32_t acc, uint16_t scale, unsigned shift, bool relu) {
    __int128 v = __int128(acc) * scale;
    if (shift) v += __int128(1) << (shift - 1);
    v >>= shift;   // arithmetic
    if (v > 127) v = 127;
    if (v < -128) v = -128;
    if (relu && v < 0) v = 0;
    return int8_t(v);
}

namespace {
double seconds(std::chrono::steady_clock::time_point t) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - t).count();
}

void arrayMultiplier() {
    for (size_t w : {4u, 8u, 16u, 32u}) for (bool compact : {false, true}) {
        const auto start = std::chrono::steady_clock::now();
        Rig r(circuit::families::arrayMultiplier(w), compact);
        const uint64_t mask = (uint64_t{1} << w) - 1, sign = uint64_t{1} << (w - 1);
        std::vector<std::pair<uint64_t, uint64_t>> pairs;
        if (w == 4) { for (uint64_t a = 0; a < 16; ++a) for (uint64_t b = 0; b < 16; ++b) pairs.push_back({a, b}); }
        else {
            for (uint64_t a : {uint64_t{0}, uint64_t{1}, mask, sign, sign - 1, sign + 1, uint64_t{3}})
                for (uint64_t b : {uint64_t{0}, uint64_t{1}, mask, sign, sign - 1, uint64_t{5}}) pairs.push_back({a, b});
            std::mt19937_64 rng(0x6a11 + w);
            const unsigned random = w == 8 ? (compact ? 600 : 200) : compact ? 200 : 40;
            for (unsigned i = 0; i < random; ++i) pairs.push_back({rng() & mask, rng() & mask});
        }
        size_t count = 0;
        for (auto [a, b] : pairs) for (unsigned mode = 0; mode < 4; ++mode) {
            const bool sa = mode & 1, sb = mode & 2;
            r.drive({{"A", a}, {"B", b}, {"SIGNED_A", sa}, {"SIGNED_B", sb}});
            const uint64_t expected = referenceProduct(a, b, sa, sb, w);
            const uint64_t got = r.out("LO") | (r.out("HI") << w);
            require(got == expected, "ArrayMultiplier<" + std::to_string(w) + "> " + (compact ? "compact" : "gates") +
                " a=" + std::to_string(a) + " b=" + std::to_string(b) + " mode=" + std::to_string(mode) +
                " got " + std::to_string(got) + " expected " + std::to_string(expected));
            ++count;
        }
        std::cout << "ArrayMultiplier<" << w << "> " << (compact ? "compact" : "gate level") << ": " << count
                  << " products, " << seconds(start) << " s\n";
    }
}

// The RV32IM M semantics, independent of the circuits: op 0..3 multiply
// (MUL, MULH, MULHSU, MULHU), 4..7 divide (DIV, DIVU, REM, REMU).
uint64_t referenceM(unsigned op, uint64_t a, uint64_t b, size_t w) {
    const uint64_t mask = w == 64 ? ~uint64_t{0} : (uint64_t{1} << w) - 1;
    const int64_t sa = signExtend(a, w), sb = signExtend(b, w);
    if (op < 4) {
        const __int128 x = op == 3 ? __int128(a) : __int128(sa);
        const __int128 y = op == 1 ? __int128(sb) : __int128(b);
        const __int128 p = x * y;
        return uint64_t(op ? p >> w : p) & mask;
    }
    if (!b) return op < 6 ? mask : a;
    const int64_t min = -(int64_t{1} << (w - 1));
    if (op == 4) return (sa == min && sb == -1) ? a : uint64_t(sa / sb) & mask;
    if (op == 5) return a / b;
    if (op == 6) return (sa == min && sb == -1) ? 0 : uint64_t(sa % sb) & mask;
    return a % b;
}

void arrayMulDiv() {
    for (size_t w : {4u, 8u, 32u}) for (bool compact : {false, true}) {
        if (w == 8 && !compact) continue;
        const auto start = std::chrono::steady_clock::now();
        Rig r(circuit::families::arrayMulDivUnit(w), compact);
        r.drive({{"ENABLE", 1}});
        const uint64_t mask = (uint64_t{1} << w) - 1, sign = uint64_t{1} << (w - 1);
        std::vector<std::pair<uint64_t, uint64_t>> pairs;
        if (w == 4) { for (uint64_t a = 0; a < 16; ++a) for (uint64_t b = 0; b < 16; ++b) pairs.push_back({a, b}); }
        else {
            for (uint64_t a : {uint64_t{0}, uint64_t{1}, mask, sign, sign - 1, uint64_t{7}})
                for (uint64_t b : {uint64_t{0}, uint64_t{1}, mask, sign, uint64_t{3}}) pairs.push_back({a, b});
            std::mt19937_64 rng(0xa77 + w);
            for (int i = 0; i < (compact ? 40 : 8); ++i) pairs.push_back({rng() & mask, rng() & mask});
        }
        size_t count = 0;
        for (auto [a, b] : pairs) for (unsigned op = 0; op < 8; ++op) {
            if (w == 32 && !compact && op >= 4 && count % 3) { ++count; continue; }   // gate-level divides are slow
            require(r.out("READY"), "ArrayMulDivUnit not ready");
            r.drive({{"A", a}, {"B", b}, {"OP", op}, {"START", 1}});
            r.cycle();
            r.drive({{"START", 0}});
            const size_t edges = op < 4 ? 1 : w;     // after the launch edge
            for (size_t e = 0; e < edges; ++e) { require(!r.out("DONE"), "ArrayMulDivUnit finished early"); r.cycle(); }
            require(r.out("DONE"), "ArrayMulDivUnit did not finish on time (op " + std::to_string(op) + ")");
            require(r.out("RESULT") == referenceM(op, a, b, w), "ArrayMulDivUnit<" + std::to_string(w) + "> op " + std::to_string(op) +
                    " a=" + std::to_string(a) + " b=" + std::to_string(b) + (compact ? " (compact)" : " (gates)"));
            r.drive({{"TAKE", 1}}); r.cycle(); r.drive({{"TAKE", 0}});
            ++count;
        }
        std::cout << "ArrayMulDivUnit<" << w << "> " << (compact ? "compact" : "gate level") << ": " << count
                  << " operations, multiply in 2 edges, " << seconds(start) << " s\n";
    }
}

void macCell() {
    for (bool compact : {false, true}) {
        Rig r(circuit::families::MacCell, compact);
        std::mt19937 rng(0x3ac);
        const std::vector<int> edge{-128, -127, -1, 0, 1, 2, 126, 127};
        int32_t sum = 0;
        int weight = 0;
        auto check = [&](int x, int32_t sum_in, int w_now, bool shift, int w_in) {
            r.drive({{"X_IN", uint8_t(x)}, {"SUM_IN", uint32_t(sum_in)}, {"SHIFT", shift}, {"W_IN", uint8_t(w_in)}});
            r.cycle();
            const int32_t expected = int32_t(uint32_t(sum_in) + uint32_t(int32_t(x) * w_now));
            require(int32_t(uint32_t(r.out("SUM_OUT"))) == expected, "MacCell sum mismatch x=" + std::to_string(x) + " w=" + std::to_string(w_now));
            require(int8_t(r.out("X_OUT")) == int8_t(x), "MacCell X_OUT mismatch");
            if (shift) weight = int8_t(w_in);
            require(int8_t(r.out("W_OUT")) == int8_t(weight), "MacCell weight mismatch");
        };
        for (int w : edge) {
            check(0, 0, weight, true, w);            // load the weight
            for (int x : edge) for (int32_t s : {0, 1, -1, 0x7fffffff, int32_t(0x80000000), 123456}) check(x, s, weight, false, 0);
        }
        for (int i = 0; i < 200; ++i) {
            const bool shift = rng() % 5 == 0;
            const int w_in = int(int8_t(rng()));
            check(int(int8_t(rng())), int32_t(rng()), weight, shift, w_in);
        }
        (void)sum;
        std::cout << "MacCell " << (compact ? "compact" : "gate level") << ": edge weights, wraps and 200 random edges\n";
    }
}

void requantizer() {
    for (bool compact : {false, true}) {
        const auto start = std::chrono::steady_clock::now();
        Rig r(circuit::families::Requantizer, compact);
        std::mt19937 rng(0x4e9);
        size_t count = 0;
        auto check = [&](int32_t acc, uint16_t scale, unsigned shift, bool relu) {
            r.drive({{"ACC", uint32_t(acc)}, {"SCALE", scale}, {"SHIFT", shift}, {"RELU", relu}});
            const int8_t expected = referenceRequantize(acc, scale, shift, relu);
            require(int8_t(r.out("OUT")) == expected, "Requantizer acc=" + std::to_string(acc) + " scale=" + std::to_string(scale) +
                " shift=" + std::to_string(shift) + " relu=" + std::to_string(relu) + " got " + std::to_string(int8_t(r.out("OUT"))) +
                " expected " + std::to_string(expected));
            ++count;
        };
        for (int32_t acc : {0, 1, -1, 127, 128, -128, -129, 255, 256, 1000, -1000, 0x7fffffff, int32_t(0x80000000)})
            for (uint16_t scale : {uint16_t(0), uint16_t(1), uint16_t(2), uint16_t(255), uint16_t(256), uint16_t(0xffff)})
                for (unsigned shift : {0u, 1u, 2u, 7u, 8u, 15u, 16u, 30u, 31u})
                    for (bool relu : {false, true}) check(acc, scale, shift, relu);
        for (int i = 0; i < (compact ? 3000 : 400); ++i)
            check(int32_t(rng()) >> (rng() % 24), uint16_t(rng()), rng() % 32, rng() & 1);
        std::cout << "Requantizer " << (compact ? "compact" : "gate level") << ": " << count << " values, " << seconds(start) << " s\n";
    }
}

// Drives one systolic array with random weights and vectors and checks every
// output vector, and that it appears exactly 2N-2 edges after its input.
void systolicArray(size_t n, bool compact, unsigned vectors) {
    const auto start = std::chrono::steady_clock::now();
    Rig r(circuit::families::systolicArray(n), compact);
    std::mt19937 rng(0x5a + n);
    std::vector<std::vector<int>> w(n, std::vector<int>(n));
    for (auto& row : w) for (auto& v : row) v = int(int8_t(rng()));
    // Load: at the k-th edge the top row receives W[N-1-k].
    std::map<std::string, uint64_t> drive;
    for (size_t k = 0; k < n; ++k) {
        drive = {{"SHIFT", 1}};
        for (size_t j = 0; j < n; ++j) drive["W_IN_" + std::to_string(j)] = uint8_t(w[n - 1 - k][j]);
        r.drive(drive);
        r.cycle();
    }
    drive = {{"SHIFT", 0}};
    for (size_t j = 0; j < n; ++j) drive["W_IN_" + std::to_string(j)] = 0;
    r.drive(drive);
    const size_t latency = systolicLatency(n);
    std::vector<std::vector<int>> xs;
    for (unsigned v = 0; v < vectors; ++v) {
        std::vector<int> x(n);
        for (auto& e : x) e = v % 7 == 3 ? (rng() & 1 ? 127 : -128) : int(int8_t(rng()));
        xs.push_back(x);
    }
    // Feed one vector per edge, then zeros; output e is checked after edge e + latency.
    for (size_t edge = 0; edge < vectors + latency; ++edge) {
        std::map<std::string, uint64_t> in;
        for (size_t i = 0; i < n; ++i) in["X_IN_" + std::to_string(i)] = edge < vectors ? uint8_t(xs[edge][i]) : 0;
        r.drive(in);
        r.cycle();
        if (edge >= latency) {
            const auto& x = xs[edge - latency];
            for (size_t j = 0; j < n; ++j) {
                int32_t expected = 0;
                for (size_t i = 0; i < n; ++i) expected += x[i] * w[i][j];
                require(int32_t(uint32_t(r.out("Y_OUT_" + std::to_string(j)))) == expected,
                        "SystolicArray<" + std::to_string(n) + "> column " + std::to_string(j) + " of vector " +
                        std::to_string(edge - latency) + (compact ? " (compact)" : " (gates)"));
            }
        }
    }
    std::cout << "SystolicArray<" << n << "> " << (compact ? "compact" : "gate level") << ": " << vectors
              << " vectors, latency " << latency << ", " << seconds(start) << " s\n";
}

// The one unknown case programs reach: an unwritten (X) activation byte
// meeting a zero weight. The gates give exactly 0 for that product; the
// compact array must give the same known sums.
void unknownPadding(bool compact) {
    Rig r(circuit::families::systolicArray(2), compact);
    // W = [[3, -2], [0, 0]]: row 1 (the padding input) has zero weights.
    r.drive({{"SHIFT", 1}, {"W_IN_0", 0}, {"W_IN_1", 0}}); r.cycle();
    r.drive({{"W_IN_0", 3}, {"W_IN_1", uint8_t(-2)}}); r.cycle();
    r.drive({{"SHIFT", 0}, {"W_IN_0", 0}, {"W_IN_1", 0}, {"X_IN_0", 7}});
    r.unknown("X_IN_1");
    r.cycle();
    r.drive({{"X_IN_0", 0}, {"X_IN_1", 0}});
    r.cycle();
    r.cycle();   // the vector's result is out after its edge + 2N - 2
    require(int32_t(uint32_t(r.out("Y_OUT_0"))) == 21 && int32_t(uint32_t(r.out("Y_OUT_1"))) == -14,
            std::string("Unknown padding times zero weights is not 0") + (compact ? " (compact)" : " (gates)"));
    std::cout << "SystolicArray<2> " << (compact ? "compact" : "gate level") << ": unknown activation x zero weight = 0\n";
}

void systolicArrays() {
    unknownPadding(false); unknownPadding(true);
    systolicArray(2, false, 24); systolicArray(2, true, 24);
    systolicArray(4, false, 12); systolicArray(4, true, 40);
    systolicArray(8, true, 24);
    systolicArray(16, true, 12);
}

class CheckCase : public StandaloneVerificationTest {
    std::string name_;
    std::function<void()> check_;
public:
    CheckCase(std::string n, std::function<void()> f) : name_(std::move(n)), check_(std::move(f)) {}
    std::string getTestName() const override { return name_; }
    void verifyResults() override {
        BasicComponent::resetSetupViolations();
        check_();
        require(BasicComponent::setupViolations() == 0, "Component check observed setup violations");
    }
};

// Recorded scenarios for the visualizer: inputs and settled outputs per step.
class Steps {
    std::map<std::string, size_t> widths_;
    circuit::test::NamedValues state_;
public:
    circuit::test::ComponentTestSpec spec;
    // after_reset: the outputs expected once reset has been released.
    Steps(const std::string& name, const std::string& contract, const std::string& instance,
          const std::map<std::string, uint64_t>& after_reset = {})
        : spec{name, contract, instance, {}, {{"contract", {}, {}, {}}}} {
        for (const auto& p : builtinComponentCatalog().contract(contract).pinsFor({})) {
            widths_[p.name] = p.width;
            if (p.direction == PinType::INPUT) state_[p.name] = circuit::test::logicBits(p.width, 0);
        }
        if (state_.contains("RST")) {
            step("reset", {{"RST", 1}}, {});
            cycle("reset-edge", {}, {}, false);
            step("ready", {{"RST", 0}}, after_reset);
        }
    }
    void step(const std::string& id, const std::map<std::string, uint64_t>& changes,
              const std::map<std::string, uint64_t>& expected, bool emit = true) {
        for (const auto& [n, v] : changes) state_.at(n) = circuit::test::logicBits(widths_.at(n), v);
        circuit::test::NamedValues outputs;
        for (const auto& [n, v] : expected) outputs[n] = circuit::test::logicBits(widths_.at(n), v);
        spec.scenarios[0].actions.push_back({id, circuit::test::CheckpointKind::Settled, state_, outputs, emit, {}});
    }
    void cycle(const std::string& id, std::map<std::string, uint64_t> changes,
               const std::map<std::string, uint64_t>& expected, bool emit = true) {
        changes["CLK"] = 0;
        step("", changes, {}, false);
        step(id, {{"CLK", 1}}, expected, emit);
        step("", {{"CLK", 0}}, {}, false);
    }
};

circuit::test::ComponentTestSpec multiplierScenario() {
    Steps s("ArrayMultiplierWidth4ScenarioTest", "arithmetic.array-multiplier.4", "ARRAY_MULTIPLIER_W4_ROOT");
    for (auto [a, b, sa, sb] : std::vector<std::tuple<uint64_t, uint64_t, bool, bool>>{
             {7, 5, false, false}, {15, 15, false, false}, {15, 15, true, true}, {8, 7, true, true},
             {8, 8, true, true}, {9, 3, true, false}, {12, 13, false, true}, {0, 11, true, true}}) {
        const auto p = referenceProduct(a, b, sa, sb, 4);
        // The label names each operand's value under its sign flag: 15 is −1 when signed.
        const auto value = [](uint64_t v, bool sign) { return std::to_string(sign ? signExtend(v, 4) : int64_t(v)); };
        s.step(value(a, sa) + "x" + value(b, sb) + (sa ? "s" : "u") + (sb ? "s" : "u"),
               {{"A", a}, {"B", b}, {"SIGNED_A", sa}, {"SIGNED_B", sb}}, {{"LO", p & 15}, {"HI", p >> 4}});
    }
    return s.spec;
}
// The four multiplies finish 2 edges after START, a divide W + 1 = 5; the
// first divide shows each step. Then the divide edge cases (by zero, signed
// remainder, the one signed overflow) and a divide discarded by CANCEL.
circuit::test::ComponentTestSpec mulDivScenario() {
    Steps s("ArrayMulDivUnitWidth4ScenarioTest", "arithmetic.array-mul-div.4", "ARRAY_MULDIV_W4_ROOT",
            {{"BUSY", 0}, {"DONE", 0}});
    s.step("enable", {{"ENABLE", 1}}, {{"READY", 1}, {"BUSY", 0}, {"DONE", 0}});
    const char* names[] = {"mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu"};
    // An operand as the operation reads it: A is signed for MULH, MULHSU, DIV
    // and REM, B for MULH, DIV and REM. "neg3" is −3.
    const auto value = [](uint64_t v, bool sign) {
        const int64_t x = sign ? signExtend(v, 4) : int64_t(v);
        return x < 0 ? "neg" + std::to_string(-x) : std::to_string(x);
    };
    const auto busy = std::map<std::string, uint64_t>{{"BUSY", 1}, {"DONE", 0}, {"READY", 0}};
    bool shown_steps = false;
    for (auto [op, a, b] : std::vector<std::tuple<unsigned, uint64_t, uint64_t>>{
             {0, 7, 5}, {1, 13, 5}, {2, 8, 3}, {3, 15, 15}, {4, 9, 2}, {5, 9, 0}, {6, 9, 3}, {4, 8, 15}}) {
        const bool sa = op == 1 || op == 2 || op == 4 || op == 6, sb = op == 1 || op == 4 || op == 6;
        const std::string id = std::string(names[op]) + "-" + value(a, sa) + "-" + value(b, sb);
        s.cycle(id + "-launch", {{"A", a}, {"B", b}, {"OP", op}, {"START", 1}}, busy);
        const bool steps = op >= 4 && !shown_steps;
        for (unsigned e = 1; op >= 4 && e < 4; ++e)
            s.cycle(id + "-step-" + std::to_string(e), {{"START", 0}}, busy, steps);
        shown_steps = shown_steps || steps;
        s.cycle(id + "-done", {{"START", 0}}, {{"BUSY", 0}, {"DONE", 1}, {"READY", 0}, {"RESULT", referenceM(op, a, b, 4)}});
        s.cycle(id + "-take", {{"TAKE", 1}}, {{"BUSY", 0}, {"DONE", 0}, {"READY", 1}});
        s.step("", {{"TAKE", 0}}, {}, false);
    }
    // CANCEL discards a divide one step in at the next edge: nothing runs or
    // is held; a launch is accepted again once CANCEL is released.
    s.cycle("cancel-launch", {{"A", 9}, {"B", 2}, {"OP", 4}, {"START", 1}}, busy);
    s.cycle("cancel-step-1", {{"START", 0}}, busy);
    s.cycle("cancel", {{"CANCEL", 1}}, {{"BUSY", 0}, {"DONE", 0}});
    s.step("cancel-released", {{"CANCEL", 0}}, {{"BUSY", 0}, {"DONE", 0}, {"READY", 1}});
    return s.spec;
}
circuit::test::ComponentTestSpec macScenario() {
    Steps s("MacCellScenarioTest", "accelerator.mac-cell", "MAC_CELL_ROOT", {{"W_OUT", 0}, {"X_OUT", 0}, {"SUM_OUT", 0}});
    s.cycle("load-weight-3", {{"SHIFT", 1}, {"W_IN", 3}}, {{"W_OUT", 3}, {"X_OUT", 0}, {"SUM_OUT", 0}});
    s.cycle("5x3-plus-10", {{"SHIFT", 0}, {"W_IN", 0}, {"X_IN", 5}, {"SUM_IN", 10}}, {{"SUM_OUT", 25}, {"X_OUT", 5}, {"W_OUT", 3}});
    s.cycle("neg2x3-plus-25", {{"X_IN", uint8_t(-2)}, {"SUM_IN", 25}}, {{"SUM_OUT", 19}, {"X_OUT", uint8_t(-2)}, {"W_OUT", 3}});
    // The sum uses the weight held before the edge: 0 × 3 here, while −128 loads.
    s.cycle("load-weight-neg128", {{"SHIFT", 1}, {"W_IN", 0x80}, {"X_IN", 0}, {"SUM_IN", 0}}, {{"W_OUT", 0x80}, {"X_OUT", 0}, {"SUM_OUT", 0}});
    s.cycle("neg128-squared", {{"SHIFT", 0}, {"X_IN", 0x80}, {"SUM_IN", 0}}, {{"SUM_OUT", 16384}, {"X_OUT", 0x80}, {"W_OUT", 0x80}});
    return s.spec;
}
circuit::test::ComponentTestSpec requantizeScenario() {
    Steps s("RequantizerScenarioTest", "accelerator.requantize", "REQUANTIZER_ROOT");
    for (auto [acc, scale, shift, relu] : std::vector<std::tuple<int32_t, uint16_t, unsigned, bool>>{
             {1000, 1, 4, false}, {1000, 1, 3, false}, {-1000, 1, 4, false}, {-1000, 1, 4, true},
             {300, 3, 2, false}, {5, 1, 1, false}, {-5, 1, 1, false}, {40000, 1, 0, false},
             {-40000, 1, 0, false}}) {
        const auto out = referenceRequantize(acc, scale, shift, relu);
        s.step(std::to_string(acc) + "*" + std::to_string(scale) + ">>" + std::to_string(shift) + (relu ? "-relu" : ""),
               {{"ACC", uint32_t(acc)}, {"SCALE", scale}, {"SHIFT", shift}, {"RELU", relu}}, {{"OUT", uint8_t(out)}});
    }
    return s.spec;
}
circuit::test::ComponentTestSpec arrayScenario() {
    // W = [[1, 2], [3, 4]]; x = (5, 6) → y = (5·1 + 6·3, 5·2 + 6·4) = (23, 34).
    // Every register starts at 0 after reset and the inputs are 0 until x
    // enters, so the outputs stay 0 until the first result, 2N − 2 = 2 edges later.
    const std::map<std::string, uint64_t> zero{{"Y_OUT_0", 0}, {"Y_OUT_1", 0}};
    Steps s("SystolicArrayWidth2ScenarioTest", "accelerator.systolic-array.2", "SYSTOLIC_ARRAY_2_ROOT", zero);
    s.cycle("shift-row-1", {{"SHIFT", 1}, {"W_IN_0", 3}, {"W_IN_1", 4}}, zero);
    s.cycle("shift-row-0", {{"W_IN_0", 1}, {"W_IN_1", 2}}, zero);
    s.cycle("x-5-6", {{"SHIFT", 0}, {"W_IN_0", 0}, {"W_IN_1", 0}, {"X_IN_0", 5}, {"X_IN_1", 6}}, zero);
    s.cycle("x-1-0", {{"X_IN_0", 1}, {"X_IN_1", 0}}, zero);
    s.cycle("y-23-34", {{"X_IN_0", 0}, {"X_IN_1", 0}}, {{"Y_OUT_0", 23}, {"Y_OUT_1", 34}});
    s.cycle("y-1-2", {}, {{"Y_OUT_0", 1}, {"Y_OUT_1", 2}});
    return s.spec;
}
} // namespace

void appendStageOneTwo(std::vector<TestRegistryEntry>& entries) {
    auto add = [&](std::string name, std::function<void()> check) {
        entries.push_back({name, name, RegisteredTestKind::Integration, {}, {}, {"simulation", "accelerator"},
            [name, check] { return std::make_unique<CheckCase>(name, check); }, false});
    };
    add("ArrayMultiplierTest", arrayMultiplier);
    add("ArrayMulDivUnitTest", arrayMulDiv);
    add("MacCellTest", macCell);
    add("RequantizerTest", requantizer);
    add("SystolicArrayTest", systolicArrays);
    // A recorded scenario belongs to its component's one logical test.
    struct Recorded { std::string name, logical, contract; std::function<circuit::test::ComponentTestSpec()> spec; RegisteredTestKind kind; };
    for (const auto& r : std::vector<Recorded>{
             {"ArrayMultiplierWidth4ScenarioTest", "ArrayMultiplierTest", "arithmetic.array-multiplier.4", multiplierScenario, RegisteredTestKind::TruthTable},
             {"ArrayMulDivUnitWidth4ScenarioTest", "ArrayMulDivUnitTest", "arithmetic.array-mul-div.4", mulDivScenario, RegisteredTestKind::Sequential},
             {"MacCellScenarioTest", "MacCellTest", "accelerator.mac-cell", macScenario, RegisteredTestKind::Sequential},
             {"RequantizerScenarioTest", "RequantizerTest", "accelerator.requantize", requantizeScenario, RegisteredTestKind::TruthTable},
             {"SystolicArrayWidth2ScenarioTest", "SystolicArrayTest", "accelerator.systolic-array.2", arrayScenario, RegisteredTestKind::Sequential}}) {
        const auto make = r.spec;
        entries.push_back({r.name, r.logical, r.kind, {r.contract}, "contract", {"simulation", "accelerator-scenario"},
            [make] { return std::make_unique<circuit::test::ComponentScenarioTest>(make(), "contract", circuit::strictAllStructural()); }, true});
    }
}

} // namespace circuit::accelerator::test

void appendAcceleratorTests(std::vector<TestRegistryEntry>& entries) {
    circuit::accelerator::test::appendStageOneTwo(entries);
    circuit::accelerator::test::appendDeviceTests(entries);
}
