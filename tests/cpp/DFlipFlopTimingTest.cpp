// The gate-level D flip-flop is a loop of latches, so the timing analysis
// cannot bound it by following paths. It is small enough to try everything:
// every stored value, every old and new data value and every offset of a data
// change around the clock edge. The numbers found here are the contract the
// analysis uses for it (modules/basic/DFlipFlopTiming.hpp).
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/DFlipFlopTiming.hpp"
#include "simulator/Event.hpp"
#include "simulator/SimulationTest.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
constexpr size_t WINDOW = 24, QUIET = 60;

struct Bench {
    std::shared_ptr<IOComponent> root = Component::create<IOComponent>("BENCH");
    std::shared_ptr<DFlipFlop> flop;
    std::shared_ptr<Wire<>> d, clk, rst, q, q_bar;
    Simulator sim;
    size_t now = 0;
    Bench() {
        ComponentBuilder b(root);
        flop = b.addNewComponent<DFlipFlop>("FLOP");
        d = b.addNewWire("d", nullptr, {flop->getInputPin("D")});
        clk = b.addNewWire("clk", nullptr, {flop->getInputPin("CLK")});
        rst = b.addNewWire("rst", nullptr, {flop->getInputPin("RST")});
        q = b.addNewWire("q", flop->getOutputPin("Q"), {});
        q_bar = b.addNewWire("q_bar", flop->getOutputPin("Q_BAR"), {});
        sim.setHistoryRecordingEnabled(false);
        SimulationTest::scheduleInitialEventsForTree(root, sim, 0);
    }
    void set(const std::shared_ptr<Wire<>>& wire, bool value, size_t at) {
        sim.scheduleEvent(std::make_shared<WireUpdateEvent<>>(at, wire, value ? LogicValue::HIGH : LogicValue::LOW));
    }
    void data(LogicValue value, size_t at) { sim.scheduleEvent(std::make_shared<WireUpdateEvent<>>(at, d, value)); }
    bool high(const std::shared_ptr<Wire<>>& wire) const { return wire->getValueVector()[0] == LogicValue::HIGH; }
    // Leave the flip-flop holding `stored`, with the clock low and D at `data`.
    void prepare(bool stored, LogicValue resting) {
        set(rst, false, now); set(clk, false, now); set(d, stored, now);
        sim.advanceAndRecord(now += QUIET);
        set(clk, true, now); sim.advanceAndRecord(now += QUIET);
        set(clk, false, now); data(resting, now);
        sim.advanceAndRecord(now += QUIET);
        if (high(q) != stored || high(q_bar) == stored) throw std::runtime_error("Flip-flop could not be prepared");
    }
};

// What a flip-flop holds after capturing a data value: X or Z become X.
LogicValue wanted(LogicValue data) {
    return data == LogicValue::LOW || data == LogicValue::HIGH ? data : LogicValue::UNKNOWN;
}
const LogicValue VALUES[] = {LogicValue::LOW, LogicValue::HIGH, LogicValue::UNKNOWN, LogicValue::HIGH_Z};

struct Edge {
    LogicValue captured = LogicValue::UNKNOWN; // the value Q ends at
    bool clean = true;     // quiet at the end, and Q and Q_BAR agree with each other
    size_t first = 0, last = 0; // first and last output change after the edge (0: none)
};
// A rising edge. D changes from `before` to `after` at `offset` ticks
// relative to the edge (negative: earlier). The clock stays high `high_for`.
Edge edge(Bench& bench, bool stored, LogicValue before, LogicValue after, long offset, size_t high_for = QUIET, size_t low_for = QUIET) {
    bench.prepare(stored, before);
    // An optional short low phase right before the edge.
    if (low_for < QUIET) {
        bench.set(bench.clk, true, bench.now); bench.sim.advanceAndRecord(bench.now += QUIET);
        if (bench.q->getValueVector()[0] != wanted(before)) throw std::runtime_error("Unexpected preparation");
        bench.set(bench.clk, false, bench.now + WINDOW - low_for);
    }
    const size_t at = bench.now + WINDOW;
    bench.data(after, static_cast<size_t>(static_cast<long>(at) + offset));
    bench.set(bench.clk, true, at);
    bench.set(bench.clk, false, at + high_for);
    bench.sim.advanceAndRecord(at - 1);
    Edge result;
    bool q = bench.high(bench.q), q_bar = bench.high(bench.q_bar);
    for (size_t tick = 0; tick <= WINDOW + QUIET; ++tick) {
        bench.sim.advanceAndRecord(at + tick);
        if (bench.high(bench.q) != q || bench.high(bench.q_bar) != q_bar) {
            if (!result.first) result.first = tick;
            result.last = tick;
            q = bench.high(bench.q); q_bar = bench.high(bench.q_bar);
        }
    }
    bench.now = at + WINDOW + QUIET;
    bench.sim.advanceAndRecord(bench.now += QUIET);
    result.captured = bench.q->getValueVector()[0];
    const auto bar = bench.q_bar->getValueVector()[0];
    // 0 and 1 come with their complement; unknown data gives X on both.
    result.clean = !bench.sim.hasPendingEvents()
        && (result.captured == LogicValue::UNKNOWN ? bar == LogicValue::UNKNOWN
            : bar == (result.captured == LogicValue::HIGH ? LogicValue::LOW : LogicValue::HIGH));
    return result;
}

void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
} // namespace

int main() {
    try {
        Bench bench;
        bench.set(bench.rst, true, 0); bench.set(bench.clk, false, 0); bench.set(bench.d, false, 0);
        bench.sim.advanceAndRecord(bench.now = QUIET);
        bench.set(bench.clk, true, bench.now); bench.sim.advanceAndRecord(bench.now += QUIET);
        bench.set(bench.clk, false, bench.now); bench.set(bench.rst, false, bench.now);
        bench.sim.advanceAndRecord(bench.now += QUIET);

        // Clock-to-output, with data quiet well before the edge. Data is tried
        // in all four values: X or Z is captured as X.
        size_t earliest = WINDOW, latest = 0;
        for (int stored = 0; stored < 2; ++stored) for (const auto data : VALUES) {
            const auto result = edge(bench, stored, data, data, -static_cast<long>(WINDOW));
            check(result.clean && result.captured == wanted(data), "A quiet edge must capture its data");
            const bool changes = wanted(data) != (stored ? LogicValue::HIGH : LogicValue::LOW);
            check((result.last != 0) == changes, "The output changes exactly when the stored value does");
            if (result.last) { earliest = std::min(earliest, result.first); latest = std::max(latest, result.last); }
        }
        // Setup: the smallest lead such that every earlier data change is
        // captured AND the output is no later than with quiet data. Data that
        // arrives at the last moment is captured, but shows up late.
        size_t setup = 0, late_capture = 0;
        for (size_t lead = 0; lead <= WINDOW; ++lead)
            for (int stored = 0; stored < 2; ++stored) for (const auto before : VALUES) for (const auto data : VALUES) {
                if (before == data) continue;
                const auto result = edge(bench, stored, before, data, -static_cast<long>(lead));
                if (!result.clean || result.captured != wanted(data)) setup = lead + 1;
                else if (result.last > latest) { setup = lead + 1; late_capture = std::max(late_capture, result.last); }
            }
        // Hold: the smallest lag such that every later data change is ignored
        // and does not disturb the output either.
        size_t hold = 0;
        for (size_t lag = 0; lag <= WINDOW; ++lag)
            for (int stored = 0; stored < 2; ++stored) for (const auto data : VALUES) for (const auto next : VALUES) {
                if (next == data) continue;
                const auto result = edge(bench, stored, data, next, static_cast<long>(lag));
                if (!result.clean || result.captured != wanted(data) || result.last > latest) hold = lag + 1;
            }
        // Clock pulse widths: the shortest high and low phases that still work.
        size_t minimum_high = 0, minimum_low = 0;
        for (size_t width = 1; width <= WINDOW; ++width)
            for (int stored = 0; stored < 2; ++stored) for (const auto before : VALUES) for (const auto data : VALUES) {
                auto result = edge(bench, stored, data, data, -static_cast<long>(WINDOW), width);
                if (!result.clean || result.captured != wanted(data) || result.last > latest) minimum_high = width + 1;
                if (before == data) continue;
                // Data changes while the clock is still high; the low phase
                // alone has to let the first latch take it.
                result = edge(bench, stored, before, data, -static_cast<long>(WINDOW), QUIET, width);
                if (!result.clean || result.captured != wanted(data) || result.last > latest) minimum_low = width + 1;
            }
        std::cout << "DFlipFlop, every stored value, data value (0, 1, X, Z) and offset within " << WINDOW << " ticks of the edge:\n"
                  << "  setup " << setup << ", hold " << hold << ", clock-to-output " << earliest << ".." << latest
                  << ", shortest high " << minimum_high << ", shortest low " << minimum_low << " ticks\n"
                  << "  (data arriving later than the setup time can still be captured, with the output as late as tick "
                  << late_capture << ")\n";
        using namespace circuit::dflipflop_timing;
        check(setup == SETUP && hold == HOLD && earliest == CLOCK_TO_OUTPUT_MINIMUM && latest == CLOCK_TO_OUTPUT_MAXIMUM
              && minimum_high == MINIMUM_HIGH && minimum_low == MINIMUM_LOW,
              "DFlipFlopTiming.hpp does not match the flip-flop");
        std::cout << "DFlipFlopTimingTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "DFlipFlopTimingTest failed: " << error.what() << '\n';
        return 1;
    }
}
