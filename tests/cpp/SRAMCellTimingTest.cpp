// The gate-level SRAM cell stores its bit in a latch, a loop of two gates, so
// the timing analysis cannot bound it by following paths. It is small enough
// to try everything: both stored values, every old and new value of its four
// inputs (0, 1, X and Z each), and a second input change arriving while the
// first is still settling. The numbers found here are the contract the
// analysis uses for it (modules/memory/SRAMCellTiming.hpp).
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/memory/SRAM.hpp"
#include "modules/memory/SRAMCellTiming.hpp"
#include "simulator/Event.hpp"
#include "simulator/SimulationTest.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace {
constexpr size_t QUIET = 24;
const std::array<const char*, 4> INPUTS = {"WL", "DRIVE", "BL", "BL_BAR"};

struct Bench {
    std::shared_ptr<IOComponent> root = Component::create<IOComponent>("BENCH");
    std::shared_ptr<SRAMCell> cell;
    std::array<std::shared_ptr<Wire<>>, 4> in;
    std::shared_ptr<Wire<>> q, q_bar;
    Simulator sim;
    size_t now = 0;
    Bench() {
        ComponentBuilder b(root);
        cell = b.addNewComponent<SRAMCell>("CELL");
        for (size_t i = 0; i < 4; ++i) in[i] = b.addNewWire(INPUTS[i], nullptr, {cell->getInputPin(INPUTS[i])});
        q = b.addNewWire("q", cell->getOutputPin("Q"), {});
        q_bar = b.addNewWire("q_bar", cell->getOutputPin("Q_BAR"), {});
        sim.setHistoryRecordingEnabled(false);
        SimulationTest::scheduleInitialEventsForTree(root, sim, 0);
    }
    // Two bits per input: 0, 1, X, Z.
    void drive(unsigned vector, size_t at) {
        for (size_t i = 0; i < 4; ++i)
            sim.scheduleEvent(std::make_shared<WireUpdateEvent<>>(at, in[i], LogicValue((vector >> (2 * i)) & 3)));
    }
    std::pair<LogicValue, LogicValue> outputs() const { return {q->getValueVector()[0], q_bar->getValueVector()[0]}; }
    // Store a bit, then rest at `vector`.
    void prepare(bool stored, unsigned vector) {
        drive(1u | (1u << 2) | ((stored ? 1u : 0u) << 4) | ((stored ? 0u : 1u) << 6), now); // WL=1 DRIVE=1 BL=v BL_BAR=!v
        sim.advanceAndRecord(now += QUIET);
        drive(vector, now);
        sim.advanceAndRecord(now += QUIET);
        if (sim.hasPendingEvents()) throw std::runtime_error("The cell did not come to rest");
    }
    // Watch from `now`; returns the first and last tick with an output change (0: none).
    std::pair<size_t, size_t> watch() {
        auto seen = outputs();
        size_t first = 0, last = 0;
        const size_t start = now;
        for (size_t tick = 0; tick <= QUIET; ++tick) {
            sim.advanceAndRecord(start + tick);
            if (outputs() != seen) { if (!first) first = tick; last = tick; seen = outputs(); }
        }
        now = start + QUIET;
        if (sim.hasPendingEvents()) throw std::runtime_error("The cell did not go quiet: it oscillates");
        return {first, last};
    }
};

void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
// The four inputs as binary values only.
unsigned binary(unsigned bits) {
    unsigned vector = 0;
    for (size_t i = 0; i < 4; ++i) vector |= ((bits >> i) & 1u) << (2 * i);
    return vector;
}
std::string describe(unsigned vector) {
    std::string text;
    for (size_t i = 0; i < 4; ++i) text += std::string(i ? " " : "") + INPUTS[i] + "=" + "01XZ"[(vector >> (2 * i)) & 3];
    return text;
}
} // namespace

int main() {
    try {
        Bench bench;
        size_t earliest = QUIET, latest = 0, cases = 0, oscillating = 0;
        // One change of any inputs, from any resting state, in all four values.
        for (int stored = 0; stored < 2; ++stored) for (unsigned before = 0; before < 256; ++before)
            for (unsigned after = 0; after < 256; ++after) {
                if (before == after) continue;
                bench.prepare(stored, before);
                bench.drive(after, bench.now);
                std::pair<size_t, size_t> seen;
                try { seen = bench.watch(); }
                catch (const std::exception&) {
                    // Record it and start again from a clean cell.
                    if (oscillating++ < 6) std::cerr << "  oscillates: stored " << stored << ", inputs " << describe(before) << " -> " << describe(after) << '\n';
                    bench = Bench();
                    continue;
                }
                const auto [first, last] = seen;
                ++cases;
                if (last) { earliest = std::min(earliest, first); latest = std::max(latest, last); }
            }
        check(oscillating == 0, "A single input change must always settle");
        // A second change while the first is still settling, in binary values.
        // Too soon, and the latch can be left oscillating or late: find the
        // shortest spacing from which neither ever happens.
        size_t spacing = 1, overlapping = 0, upset = 0;
        for (size_t gap = 1; gap <= QUIET; ++gap)
            for (int stored = 0; stored < 2; ++stored) for (unsigned a = 0; a < 16; ++a) for (unsigned b = 0; b < 16; ++b)
                for (unsigned c = 0; c < 16; ++c) {
                    if (a == b || b == c) continue;
                    bench.prepare(stored, binary(a));
                    bench.drive(binary(b), bench.now);
                    bench.drive(binary(c), bench.now + gap);
                    bench.sim.advanceAndRecord(bench.now + gap - 1);
                    bench.now += gap;
                    ++overlapping;
                    bool sound = true;
                    try { sound = bench.watch().second <= latest; }
                    catch (const std::exception&) { sound = false; bench = Bench(); }
                    if (!sound) { spacing = gap + 1; ++upset; }
                }
        std::cout << "SRAMCell: " << cases << " single input changes in four values and " << overlapping
                  << " overlapping binary changes; outputs change " << earliest << ".." << latest
                  << " ticks after the last input change.\n  " << upset << " overlapping changes upset the cell; none does once "
                  << "input changes are at least " << spacing << " ticks apart\n";
        using namespace circuit::sram_cell_timing;
        check(earliest == EARLIEST && latest == LATEST && spacing == MINIMUM_SPACING, "SRAMCellTiming.hpp does not match the cell");
        std::cout << "SRAMCellTimingTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "SRAMCellTimingTest failed: " << error.what() << '\n';
        return 1;
    }
}
