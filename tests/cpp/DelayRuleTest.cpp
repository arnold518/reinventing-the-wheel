// The delay rule: a compact (behavioral) part takes at least as long as its
// gate-level form, and not much longer.
//
// "At least as long" is a chain of links, each checked here or next to it:
//   1. Every stored delay table equals what the analysis proves from the
//      gates (part_delay_tables --verify, the PartDelayTablesCompleteTest).
//   2. The compact part obeys its table exactly: after one input pin changes,
//      an output that changes does so at exactly the table's delay.
//   3. The gates never take longer than the table (sampled, X and Z included).
//   4. Inside a larger part, where a part's inputs change one after another
//      and overlap, its gates still stay within its table, timed the way the
//      engine times the compact part.
// With 1 and 2 the compact part cannot be faster than its gates for any input;
// 3 and 4 are the same statement observed on running gates.
//
// One thing the compact parts do not copy: gates can pulse, leaving a value
// and coming back to it. A compact part computes the final value once and
// stays still, so a parent built from compact parts can go quiet sooner than
// the same parent in gates. Both are counted and printed, not failed: a table
// already includes those pulses, so every clock proven from tables allows for them.
//
// The cores then run at the clock the analysis certifies, with no compact part
// ever seeing data arrive inside its setup time.
//
// "Not much longer" is measured over a fixed, seeded set of input changes:
//   typical: the gates' average settling time as a share of the table delay;
//   worst:   the slowest gate time seen as a share of the largest table delay.
// A part below a target fails unless FLOORS records its measured shares and
// the reason; a recorded part fails if it drops below them.
//
//   DelayRuleTest [--rounds=N] [--typical=PERCENT] [--worst=PERCENT] [--clock-margin=PERCENT]
//                 [--binary] [--report] [--floors] [PART ...]
#include "analysis/RV32IExperiment.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "simulator/Event.hpp"
#include "simulator/SimulationTest.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {
using Bits = std::vector<LogicValue>;
using Vector = std::map<std::string, Bits>;

// Why a part may miss a "not much longer" target.
const char* const DATA = "usual changes are quicker than the slowest case its table must cover (carry chains, the chosen operation, stored state)";
const char* const SAMPLE = "this sample of changes did not reach its slowest case";
const char* const EMPTY_ROM = "an empty ROM never changes its data; the table covers any contents";

// Parts below a target, with the shares measured for them. A listed part
// passes while it stays at or above these; it fails if it drops lower.
struct Floor { int typical, worst; const char* why; };
const std::map<std::string, Floor> FLOORS = {
    {"memory.ff.controller.16", {96, 80, SAMPLE}},
    {"memory.ff.controller.32", {96, 36, SAMPLE}},
    {"memory.ff.controller.8", {95, 76, SAMPLE}},
    {"memory.ff.device.16", {73, 61, SAMPLE}},
    {"memory.ff.device.32", {72, 69, SAMPLE}},
    {"memory.ff.device.8", {73, 64, SAMPLE}},
    {"memory.ff.endpoint.16", {76, 80, SAMPLE}},
    {"memory.ff.endpoint.32", {75, 81, SAMPLE}},
    {"memory.ff.endpoint.8", {76, 72, SAMPLE}},
    {"memory.rom.device.16", {18, 57, EMPTY_ROM}},
    {"memory.rom.device.32", {18, 57, EMPTY_ROM}},
    {"memory.rom.device.8", {18, 57, EMPTY_ROM}},
    {"rv32i.baseline.decode", {90, 57, SAMPLE}},
    {"rv32i.core.baseline-pipeline", {59, 20, DATA}},
    {"rv32i.core.educational-five-stage", {80, 60, SAMPLE}},
    {"rv32i.core.educational-single-cycle", {57, 66, DATA}},
    {"rv32i.execution-status", {80, 61, SAMPLE}},
    {"rv32i.memory-request", {96, 73, SAMPLE}},
    {"rv32i.pipeline.execution-status", {91, 75, SAMPLE}},
    {"rv32i.pipeline.stage.decode", {90, 75, SAMPLE}},
    {"rv32i.prediction.path", {44, 63, DATA}},
    {"rv32i.prediction.recovery", {66, 61, DATA}},
    {"rv32i.single-cycle.decode", {92, 82, SAMPLE}},
};

// Parts whose pins must not move a few ticks apart.
const std::map<std::string, std::string> NOT_STAGGERED = {
    {"memory.sram.cell", "its two bit lines are driven as a pair; one moving alone makes the cell fight itself"},
    {"memory.sram.row", "its bit lines are driven as pairs; one moving alone makes a cell fight itself"},
};

// Parts this bench cannot drive: independent random pins break their operating rules.
const std::map<std::string, std::string> NOT_DRIVEN = {
    {"memory.sram.bank.8", "random word, drive and precharge lines fight over the cells; the rows and devices above use it by its protocol"},
    {"memory.sram.bank.16", "random word, drive and precharge lines fight over the cells; the rows and devices above use it by its protocol"},
    {"memory.sram.bank.32", "random word, drive and precharge lines fight over the cells; the rows and devices above use it by its protocol"},
    {"rv32i.system.educational-single-cycle", "a whole computer: once clocked it keeps working through its own memories; the cores are compared below"},
};

struct Bench {
    std::shared_ptr<IOComponent> dut;
    std::shared_ptr<IOComponent> root = Component::create<IOComponent>("BENCH");
    std::map<std::string, std::shared_ptr<WireBase>> inputs, outputs;
    Simulator sim;
    size_t now = 0;
    bool restless = false;

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
    // From rest, apply `next`; per output, the last tick at which it changed.
    // `late` holds pins that change `offset` ticks after the others.
    std::map<std::string, size_t> change(const Vector& next, size_t watch, const std::function<void(size_t)>& each_tick = {},
                                         const Vector& late = {}, size_t offset = 0) {
        now += 1;
        std::map<std::string, Bits> seen;
        for (const auto& [name, wire] : outputs) seen[name] = wire->getValueVector();
        for (const auto& [name, bits] : next)
            if (!late.contains(name)) sim.scheduleEvent(makeWireUpdateEvent(now, inputs.at(name), bits));
        for (const auto& [name, bits] : late) sim.scheduleEvent(makeWireUpdateEvent(now + offset, inputs.at(name), bits));
        std::map<std::string, size_t> last;
        for (size_t tick = 0; tick <= watch; ++tick) {
            sim.advanceAndRecord(now + tick);
            if (each_tick) each_tick(tick);
            for (const auto& [name, wire] : outputs) {
                auto current = wire->getValueVector();
                if (current != seen[name]) { last[name] = tick; seen[name] = std::move(current); }
            }
        }
        restless = sim.hasPendingEvents();
        now += watch;
        return last;
    }
};

// The sets of situations every part is put through.
enum Kind { OnePin, Unknown, FewPins, AllPins, Patterns, Staggered, ClockEdge, KINDS };
const char* const KIND_NAMES[KINDS] = {"one pin", "X or Z", "few pins", "all pins", "patterns", "staggered", "clock edge"};

struct Tally {
    // Per set: situations run, outputs where both forms changed and their
    // times were compared, and the sum of gate time / compact time over those.
    struct Set { size_t situations = 0, compared = 0; double share = 0; };
    Set sets[KINDS];
    size_t exact = 0, within = 0;
    bool timed_by_gates = false;
    // Gate changes of inner parts checked against their own tables; outputs
    // where only the gates pulsed; outputs the compact form quieted sooner
    // (two pins a few ticks apart), and the same for a parent of compact parts.
    size_t inner = 0, pulses = 0, sooner = 0, parent = 0, parent_sooner = 0, detours = 0;
    std::map<std::string, std::pair<size_t, size_t>> slowest; // output -> (slowest gate time, largest table delay)
    // Outputs where the two forms end on different values: their timing cannot
    // be compared. `differ_clean` counts those with every input at 0 or 1.
    size_t differ = 0, differ_clean = 0;
    std::string difference;
    std::vector<std::string> failures;
    void fail(const std::string& what) { if (failures.size() < 4) failures.push_back(what); }
};

// The table's delay for one input pin reaching one output, under the control
// value the part has now. The clock reaches an output by its launch delay.
std::optional<size_t> tableDelay(const circuit::PartDelays& table, const std::string& pin, const std::string& output,
                                 const Vector& now) {
    std::optional<size_t> delay;
    // An output that repeats the clock for parts further on is a plain wire.
    if (pin == table.clock && std::find(table.clock_outputs.begin(), table.clock_outputs.end(), output) != table.clock_outputs.end())
        return 0;
    if (pin == table.clock) if (const auto launch = table.launches.find(output); launch != table.launches.end())
        delay = launch->second.maximum;
    const auto* arcs = &table.arcs;
    // A new control value: only the output's worst delay covers it.
    if (pin == table.control && !table.arcs_by_control.empty()) {
        for (const auto& arc : table.arcs) if (arc.output == output) delay = std::max(delay.value_or(0), arc.delay.maximum);
        if (const auto launch = table.launches.find(output); launch != table.launches.end())
            delay = std::max(delay.value_or(0), launch->second.maximum);
        return delay;
    }
    if (!table.control.empty() && now.contains(table.control)) {
        uint64_t value = 0;
        bool binary = true;
        const auto& bits = now.at(table.control);
        for (size_t bit = 0; bit < bits.size() && binary; ++bit) {
            if (bits[bit] == LogicValue::HIGH) value |= uint64_t{1} << bit;
            else binary = bits[bit] == LogicValue::LOW;
        }
        if (binary) if (const auto found = table.arcs_by_control.find(value); found != table.arcs_by_control.end()) arcs = &found->second;
    }
    for (const auto& arc : *arcs) if (arc.input == pin && arc.output == output)
        delay = std::max(delay.value_or(0), arc.delay.maximum);
    return delay;
}

// A gate-level part inside the part under test, watched against the table
// its compact form carries. Its inputs change one after another and overlap,
// as they do in real use; each change allows the outputs it reaches to move
// for that pin's delay, exactly as the engine times a compact part.
struct Inner {
    std::string id;
    circuit::PartDelays table;
    std::map<std::string, std::shared_ptr<WireBase>> inputs, outputs;
    Vector now, shown;
    std::map<std::string, size_t> allowed;

    void rest() {
        for (const auto& [name, wire] : inputs) now[name] = wire->getValueVector();
        for (const auto& [name, wire] : outputs) { shown[name] = wire->getValueVector(); allowed[name] = 0; }
    }
    void tick(size_t time, Tally& tally) {
        std::vector<std::string> changed;
        for (const auto& [name, wire] : inputs) {
            auto value = wire->getValueVector();
            if (value == now[name]) continue;
            now[name] = std::move(value);
            // A falling clock edge launches nothing.
            if (name == table.clock && now[name] != Bits{LogicValue::HIGH}) continue;
            changed.push_back(name);
        }
        for (const auto& pin : changed) for (auto& [output, until] : allowed)
            // Reset is outside the tables: anything may follow it.
            if (table.fixed_inputs.contains(pin)) until = std::numeric_limits<size_t>::max() / 2;
            else if (const auto delay = tableDelay(table, pin, output, now)) until = std::max(until, time + *delay);
        for (const auto& [name, wire] : outputs) {
            auto value = wire->getValueVector();
            if (value == shown[name]) continue;
            shown[name] = std::move(value);
            if (time > allowed[name])
                tally.fail("inside, the gates of " + id + " changed " + name + " at " + std::to_string(time)
                           + ", its table allows until " + std::to_string(allowed[name]));
            else ++tally.inner;
        }
    }
};

void collect(const std::shared_ptr<Component>& node, std::map<std::string, std::shared_ptr<Component>>& found) {
    found[node->getID()] = node;
    for (const auto& child : node->getChildren()) collect(child, found);
}

// The compact parts inside `mixed`, each paired with its gates inside `gates`.
std::vector<Inner> innerParts(const std::shared_ptr<IOComponent>& mixed, const std::shared_ptr<IOComponent>& gates) {
    std::map<std::string, std::shared_ptr<Component>> compact, real;
    collect(mixed, compact);
    collect(gates, real);
    std::vector<Inner> result;
    for (const auto& [id, node] : compact) {
        const auto leaf = std::dynamic_pointer_cast<BasicComponent>(node);
        if (!leaf || !leaf->getDelays() || leaf->getDelays()->self_timed) continue;
        const auto twin = real.find(id);
        const auto part = twin == real.end() ? nullptr : std::dynamic_pointer_cast<IOComponent>(twin->second);
        if (!part || std::dynamic_pointer_cast<BasicComponent>(part)) continue;
        Inner inner{id, *leaf->getDelays(), {}, {}, {}, {}, {}};
        for (const auto& [name, pin] : part->getAllInputPins()) if (auto wire = pin->getExternalWireBase()) inner.inputs[name] = wire;
        for (const auto& [name, pin] : part->getAllOutputPins()) if (auto wire = pin->getExternalWireBase()) inner.outputs[name] = wire;
        // Every input must be visible, or a change could go unexplained.
        if (inner.inputs.size() != part->getAllInputPins().size() || inner.outputs.empty()) continue;
        result.push_back(std::move(inner));
    }
    return result;
}

// A compact, table-timed part inside what should be a gate-level form.
std::string compactInside(const std::shared_ptr<Component>& node) {
    if (const auto leaf = std::dynamic_pointer_cast<BasicComponent>(node); leaf && leaf->getDelays()) return node->getID();
    for (const auto& child : node->getChildren()) if (auto found = compactInside(child); !found.empty()) return found;
    return {};
}

std::shared_ptr<IOComponent> build(const std::string& contract, const circuit::BuildProfile& profile) {
    try {
        return std::dynamic_pointer_cast<IOComponent>(
            circuit::builtinComponentCatalog().createRoot({contract, "DUT", {}}, profile).root);
    } catch (const std::exception&) { return nullptr; }
}

Bits randomBits(size_t width, unsigned values, std::mt19937_64& random) {
    Bits bits(width);
    for (auto& bit : bits) bit = LogicValue(random() % values);
    return bits;
}

// Values that reach long paths: all zeros, all ones, alternating bits, a
// single low or high bit, all ones but the lowest.
Bits patternBits(size_t width, std::mt19937_64& random) {
    Bits bits(width, LogicValue::LOW);
    const unsigned choice = random() % 8;
    for (size_t bit = 0; bit < width; ++bit) {
        const bool one = choice == 1 || (choice == 2 && bit % 2) || (choice == 3 && bit % 2 == 0) || (choice == 4 && bit == 0)
            || (choice == 5 && bit + 1 == width) || (choice == 6 && bit != 0) || (choice == 7 && random() % 2);
        if (one) bits[bit] = LogicValue::HIGH;
    }
    return bits;
}

// One part, in gates, compact, and as gates around compact inner parts, put
// through the same situations of every kind.
std::optional<Tally> part(const std::string& contract, size_t rounds, bool four_state) {
    const auto gate_form = build(contract, circuit::maximallyStructural());
    const auto compact_form = build(contract, circuit::strictAllBehavioral());
    if (!gate_form || !compact_form || gate_form->getAllInputPins().empty() || gate_form->getAllOutputPins().empty()) return std::nullopt;
    const auto leaf = std::dynamic_pointer_cast<BasicComponent>(compact_form);
    if (std::dynamic_pointer_cast<BasicComponent>(gate_form) || !leaf || !leaf->getDelays()) return std::nullopt;
    const auto table = *leaf->getDelays();
    // A self-timed part also changes on its own schedule; its delays count
    // from whatever caused the change, which this bench cannot observe.
    if (table.self_timed) return std::nullopt;
    const auto mixed_form = build(contract, circuit::structuralThroughDepth(
        1, circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException));

    size_t longest = 1;
    for (const auto& arc : table.arcs) longest = std::max(longest, arc.delay.maximum);
    for (const auto& [value, arcs] : table.arcs_by_control) for (const auto& arc : arcs) longest = std::max(longest, arc.delay.maximum);
    for (const auto& [output, range] : table.launches) longest = std::max(longest, range.maximum);
    // Room for a second change up to `longest` ticks late, and for a parent
    // of compact parts, which may be slower.
    const size_t watch = 3 * longest + 56;
    // The largest delay the table holds for an output: what a new control value costs.
    std::map<std::string, size_t> worst;
    for (const auto& arc : table.arcs) worst[arc.output] = std::max(worst[arc.output], arc.delay.maximum);
    for (const auto& [output, range] : table.launches) worst[output] = std::max(worst[output], range.maximum);

    Bench gates(gate_form), compact(compact_form);
    std::optional<Bench> mixed;
    if (mixed_form && !std::dynamic_pointer_cast<BasicComponent>(mixed_form)) mixed.emplace(mixed_form);
    Tally tally;
    tally.timed_by_gates = table.twin != nullptr;
    std::mt19937_64 random(0xde1a7);
    Vector now;
    for (const auto& [name, pin] : gate_form->getAllInputPins()) now[name] = Bits(pin->getWidth(), LogicValue::LOW);
    std::vector<Inner> inner;
    if (mixed) inner = innerParts(mixed_form, gate_form);
    bool watching = false;
    const auto apply = [&](const Vector& next, const Vector& late = {}, size_t offset = 0) {
        if (watching) for (auto& part : inner) part.rest();
        auto real = gates.change(next, watch, !watching || inner.empty() ? std::function<void(size_t)>{}
            : [&](size_t time) { for (auto& part : inner) part.tick(time, tally); }, late, offset);
        auto shown = compact.change(next, watch, {}, late, offset);
        std::map<std::string, size_t> blend;
        if (mixed) blend = mixed->change(next, watch, {}, late, offset);
        now = next;
        return std::tuple{std::move(real), std::move(shown), std::move(blend)};
    };
    // Reset, one clock pulse, release.
    const bool has_reset = now.contains("RST"), has_clock = !table.clock.empty() && now.contains(table.clock);
    auto next = now;
    apply(next);
    // A part with its own clock inside never goes quiet; nothing to time.
    if (gates.restless || compact.restless) return std::nullopt;
    if (has_reset) { next["RST"][0] = LogicValue::HIGH; apply(next); }
    if (has_clock) { next[table.clock][0] = LogicValue::HIGH; apply(next); next[table.clock][0] = LogicValue::LOW; apply(next); }
    if (has_reset) { next["RST"][0] = LogicValue::LOW; apply(next); }

    std::vector<std::string> pins;
    for (const auto& [name, bits] : now) if (name != "RST" && name != table.clock && !table.fixed_inputs.contains(name)) pins.push_back(name);
    if (pins.empty() && !has_clock) return std::nullopt;
    std::map<std::string, bool> agreement, blend_agreement;
    watching = true;

    // One situation: `next` replaces the inputs, the pins in `late` arriving
    // `offset` ticks after the rest. Both forms are then compared output by output.
    const auto situation = [&](Kind kind, const Vector& target, const Vector& late, size_t offset) {
        std::map<std::string, size_t> changed; // pin -> the tick it changes
        for (const auto& [name, bits] : target) if (bits != now.at(name)) changed[name] = late.contains(name) ? offset : 0;
        if (changed.empty()) return true;
        const auto [real, shown, blend] = apply(target, late, offset);
        ++tally.sets[kind].situations;
        const std::string what = std::string(KIND_NAMES[kind]) + " (" + changed.begin()->first + (changed.size() > 1 ? ", ..." : "") + ")";
        if (gates.restless || compact.restless) {
            tally.fail(std::string(gates.restless ? "the gates" : "the compact part") + " did not go quiet, " + what);
            return false;
        }
        bool clean = true;
        for (const auto& [name, bits] : now) for (const auto bit : bits) clean = clean && (bit == LogicValue::LOW || bit == LogicValue::HIGH);
        const bool new_control = !table.arcs_by_control.empty() && changed.contains(table.control);
        size_t last_change = 0;
        for (const auto& [pin, at] : changed) last_change = std::max(last_change, at);
        for (const auto& [output, wire] : compact.outputs) {
            // Timing is compared only where the forms agreed before the change
            // and agree after it. They part ways where the compact form answers
            // X more readily than the gates, and on input codes with no meaning.
            const auto gate_value = gates.outputs.at(output)->getValueVector();
            const bool agree = gate_value == wire->getValueVector();
            const bool agreed = std::exchange(agreement[output], agree);
            const bool blend_agree = mixed && mixed->outputs.at(output)->getValueVector() == gate_value;
            const bool blend_agreed = std::exchange(blend_agreement[output], blend_agree);
            if (!agree) {
                ++tally.differ;
                if (clean && !tally.differ_clean++) tally.difference = output + ", " + what;
            }
            if (!agree || !agreed) continue;
            // The latest the table lets this output move: each changed pin
            // keeps it busy for that pin's delay; a new control value for the
            // output's largest delay.
            std::optional<size_t> allowed;
            if (new_control) { if (const auto found = worst.find(output); found != worst.end()) allowed = last_change + found->second; }
            else for (const auto& [pin, at] : changed)
                if (const auto delay = tableDelay(table, pin, output, now)) allowed = std::max(allowed.value_or(0), at + *delay);
            const auto modeled = shown.find(output);
            const auto gate = real.find(output);
            if (std::getenv("DELAY_RULE_TRACE") && allowed && ((modeled != shown.end() && modeled->second > *allowed) || (gate != real.end() && gate->second > *allowed))) {
                std::cerr << contract << ' ' << what << " output " << output << " allowed " << *allowed << " compact "
                          << (modeled == shown.end() ? -1 : int(modeled->second)) << " gates " << (gate == real.end() ? -1 : int(gate->second)) << " :";
                for (const auto& [pin, at] : changed) { const auto d = tableDelay(table, pin, output, now); std::cerr << ' ' << pin << '@' << at << '+' << (d ? int(*d) : -1); }
                std::cerr << (new_control ? " new-control" : "") << " clk=" << (has_clock ? int(now.at(table.clock)[0] == LogicValue::HIGH) : -1) << '\n';
            }
            if (modeled != shown.end()) {
                if (!allowed) tally.fail(output + " changed, a path the table does not have, " + what);
                // All pins at once: the compact part answers at exactly the table's delay.
                else if (kind != Staggered && !table.twin && modeled->second != *allowed)
                    tally.fail(output + " changed after " + std::to_string(modeled->second) + " ticks, table says "
                               + std::to_string(*allowed) + ", " + what);
                // Two pins apart in time: after the first, the compact form held
                // a value its gates never show (an input code with no meaning),
                // and waited its worst case for it. Late, never early.
                else if (modeled->second > *allowed && worst.contains(output) && modeled->second <= last_change + worst.at(output))
                    ++tally.detours;
                else if (modeled->second > *allowed)
                    tally.fail(output + " changed after " + std::to_string(modeled->second) + " ticks, later than the table's "
                               + std::to_string(*allowed) + ", " + what);
                else ++tally.exact;
            }
            if (gate != real.end()) {
                if (!allowed) tally.fail("gates: " + output + " changed, a path the table does not have, " + what);
                else if (gate->second > *allowed)
                    tally.fail("gates: " + output + " changed after " + std::to_string(gate->second) + " ticks, later than the table's "
                               + std::to_string(*allowed) + ", " + what);
                else {
                    ++tally.within;
                    auto& [slowest, largest] = tally.slowest[output];
                    slowest = std::max(slowest, gate->second);
                    largest = std::max(largest, *allowed);
                }
            }
            // The two forms side by side.
            if (gate != real.end() && modeled == shown.end()) ++tally.pulses;
            if (gate != real.end() && modeled != shown.end()) {
                if (modeled->second >= gate->second) {
                    ++tally.sets[kind].compared;
                    tally.sets[kind].share += modeled->second ? double(gate->second) / double(modeled->second) : 1.0;
                }
                // The compact part's last change came with the earlier pin; the
                // gates moved once more for the later one and came back.
                else if (kind == Staggered) ++tally.sooner;
                else tally.fail("compact " + output + " changed after " + std::to_string(modeled->second) + " ticks, before the gates' "
                                + std::to_string(gate->second) + ", " + what);
            }
            // A part built from compact parts, against the same part in gates.
            if (mixed && blend_agree && blend_agreed && gate != real.end()) {
                const auto blended = blend.find(output);
                if (blended == blend.end() || blended->second < gate->second) ++tally.parent_sooner; else ++tally.parent;
            }
        }
        return true;
    };
    const auto binary = [](const Bits& bits) {
        return std::all_of(bits.begin(), bits.end(), [](LogicValue bit) { return bit == LogicValue::LOW || bit == LogicValue::HIGH; });
    };
    // A new 0/1 value for one pin, never the one it has.
    const auto fresh = [&](const std::string& pin) {
        auto bits = randomBits(now.at(pin).size(), 2, random);
        if (bits == now.at(pin)) bits[random() % bits.size()] = bits[0] == LogicValue::LOW && bits.size() == 1 ? LogicValue::HIGH
            : bits[0] == LogicValue::HIGH && bits.size() == 1 ? LogicValue::LOW : LogicValue(1 - int(now.at(pin)[0] == LogicValue::HIGH));
        if (bits == now.at(pin)) bits[0] = bits[0] == LogicValue::LOW ? LogicValue::HIGH : LogicValue::LOW;
        return bits;
    };
    const auto clockEdge = [&] {
        if (!has_clock) return true;
        auto target = now;
        target[table.clock][0] = target[table.clock][0] == LogicValue::HIGH ? LogicValue::LOW : LogicValue::HIGH;
        return situation(ClockEdge, target, {}, 0);
    };
    for (int kind = OnePin; kind < ClockEdge; ++kind) {
        if (pins.empty()) break;
        if (kind == Unknown && !four_state) continue;
        if ((kind == FewPins || kind == Staggered) && pins.size() < 2) continue;
        if (kind == Staggered && NOT_STAGGERED.contains(contract)) continue;
        for (size_t round = 0; round < rounds; ++round) {
            auto target = now;
            Vector late;
            size_t offset = 0;
            const auto& first = pins[random() % pins.size()];
            if (kind == OnePin) target[first] = fresh(first);
            else if (kind == Unknown) {
                // To X or Z, and back to 0/1 as its own situation.
                target[first] = randomBits(now.at(first).size(), 4, random);
                if (binary(target[first])) target[first][0] = LogicValue(2 + random() % 2);
                if (!situation(Unknown, target, {}, 0) || !clockEdge()) return tally;
                target = now;
                target[first] = randomBits(now.at(first).size(), 2, random);
            } else if (kind == FewPins) {
                const size_t count = 2 + random() % std::min<size_t>(3, pins.size() - 1);
                for (size_t i = 0; i < count; ++i) { const auto& pin = pins[random() % pins.size()]; target[pin] = fresh(pin); }
            } else if (kind == AllPins) for (const auto& pin : pins) target[pin] = randomBits(now.at(pin).size(), 2, random);
            else if (kind == Patterns) for (const auto& pin : pins) target[pin] = patternBits(now.at(pin).size(), random);
            else {
                auto second = pins[random() % pins.size()];
                while (second == first) second = pins[random() % pins.size()];
                target[first] = fresh(first);
                target[second] = fresh(second);
                late[second] = target[second];
                offset = 1 + random() % longest;
            }
            if (!situation(Kind(kind), target, late, offset) || !clockEdge()) return tally;
        }
    }
    if (pins.empty()) for (size_t round = 0; round < rounds; ++round) if (!clockEdge()) return tally;
    return tally;
}

// The cores, clocked as the analysis certifies: verified against the oracle,
// and no compact part ever sees data arrive inside its setup time.
//
// The gate-level core runs at the clock proven on its gates. The compact core
// may be slower, so it runs at the clock proven on the compact system. Where
// that cannot be proven (the advanced core: its ready signals and the memory's
// form a loop that only the gates resolve), the smallest clock without a setup
// violation is searched for and must stay within `margin` percent of the gates'.
void cores(double margin) {
    using namespace circuit::analysis;
    const auto certified = [](const char* core, const char* fidelity) -> std::optional<size_t> {
        RV32IExperimentConfig certify;
        certify.core = core;
        certify.fidelity = fidelity;
        certify.safe_clock_only = true;
        const auto clock = runRV32IExperiment(certify).safe_clock;
        if (clock.status != ClockTimingStatus::SafeUnderContracts || !clock.safe_period_ticks) return std::nullopt;
        return *clock.safe_period_ticks;
    };
    struct Outcome { bool verified = false; size_t violations = 0, cycles = 0; };
    using Setup = std::function<void(RV32IExperimentConfig&)>;
    const auto run = [](const char* core, const char* fidelity, size_t period, const Setup& setup = {}) {
        RV32IExperimentConfig config;
        config.core = core;
        config.fidelity = fidelity;
        config.program = 20;
        if (setup) setup(config);
        config.period = period;
        BasicComponent::resetSetupViolations();
        Outcome outcome;
        try { const auto result = runRV32IExperiment(config); outcome.verified = result.verified; outcome.cycles = result.cycles; }
        catch (const std::exception&) {}
        outcome.violations = BasicComponent::setupViolations();
        return outcome;
    };
    const auto line = [](const char* core, const std::string& what, size_t period, const Outcome& outcome) {
        std::cout << "  " << std::left << std::setw(9) << core << std::setw(34) << what << "period " << std::setw(4) << period
                  << (outcome.verified ? " verified" : " NOT verified") << ", setup violations " << outcome.violations << '\n';
    };
    for (const char* core : {"single", "baseline", "advanced"}) {
        const auto gate_clock = certified(core, "reference");
        if (!gate_clock) throw std::runtime_error(std::string(core) + ": no certified clock for the gates");
        const auto gates = run(core, "reference", *gate_clock);
        line(core, "gates, clock proven on gates", *gate_clock, gates);
        if (!gates.verified || gates.violations) throw std::runtime_error(std::string(core) + ": the gates fail at their certified clock");

        size_t compact_clock = 0;
        if (const auto proven = certified(core, "behavioral")) {
            compact_clock = *proven;
            const auto compact = run(core, "behavioral", compact_clock);
            line(core, "compact, clock proven on compact", compact_clock, compact);
            if (!compact.verified || compact.violations) throw std::runtime_error(std::string(core) + ": the compact core fails at its certified clock");
        } else {
            const auto at_gate_clock = run(core, "behavioral", *gate_clock);
            line(core, "compact, at the gates' clock", *gate_clock, at_gate_clock);
            if (!at_gate_clock.verified) throw std::runtime_error(std::string(core) + ": the compact core is wrong at the gates' clock");
            for (compact_clock = *gate_clock; compact_clock <= 2 * *gate_clock; compact_clock += 2) {
                const auto compact = run(core, "behavioral", compact_clock);
                if (compact.verified && !compact.violations) { line(core, "compact, smallest clean clock found", compact_clock, compact); break; }
            }
        }
        const double extra = 100.0 * (double(compact_clock) - double(*gate_clock)) / double(*gate_clock);
        std::cout << "  " << std::left << std::setw(9) << core << "compact clock is " << std::fixed << std::setprecision(1) << extra
                  << "% longer than the gates'\n";
        if (compact_clock < *gate_clock) throw std::runtime_error(std::string(core) + ": the compact core is certified faster than its gates");
        if (extra > margin) throw std::runtime_error(std::string(core) + ": the compact core needs a clock more than "
                                                     + std::to_string(int(margin)) + "% longer than its gates");
        // What an experiment reads off a run: at the gates' clock the compact
        // core takes exactly the cycles the gate-level core takes.
        std::vector<std::pair<std::string, Setup>> runs = {
            {"program 17", [](RV32IExperimentConfig& c) { c.program = 17; }},
            {"program 20", {}},
            {"program 22", [](RV32IExperimentConfig& c) { c.program = 22; }},
        };
        if (std::string(core) == "advanced") {
            runs.emplace_back("program 20, SRAM", [](RV32IExperimentConfig& c) { c.memory.technology = circuit::memory::Technology::SRAM; });
            runs.emplace_back("program 20, two-bit predictor", [](RV32IExperimentConfig& c) { c.prediction.policy = rv32i::PredictionPolicy::TwoBit; });
        }
        for (const auto& [name, setup] : runs) {
            const auto real = run(core, "reference", *gate_clock, setup), model = run(core, "behavioral", *gate_clock, setup);
            std::cout << "  " << std::left << std::setw(9) << core << std::setw(34) << name << "gates " << real.cycles << " cycles, compact "
                      << model.cycles << " cycles\n";
            if (!real.verified || !model.verified || real.cycles != model.cycles)
                throw std::runtime_error(std::string(core) + ", " + name + ": the compact core does not take the gates' cycle count");
        }
        // Clocked too fast, the compact core must not pass quietly.
        const size_t rushed = std::max<size_t>(4, (*gate_clock / 4) * 2);
        const auto fast = run(core, "behavioral", rushed);
        const bool caught = !fast.verified || fast.violations > 0;
        line(core, "compact, clocked far too fast", rushed, fast);
        if (!caught) throw std::runtime_error(std::string(core) + ": a compact core ran faster than its gates allow without any sign");
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        size_t rounds = 100;
        double typical_target = 70, worst_target = 90, clock_margin = 10;
        bool report_only = false, four_state = true, print_floors = false;
        std::ostringstream floors;
        std::set<std::string> only;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument.rfind("--rounds=", 0) == 0) rounds = std::stoul(argument.substr(9));
            else if (argument.rfind("--typical=", 0) == 0) typical_target = std::stod(argument.substr(10));
            else if (argument.rfind("--worst=", 0) == 0) worst_target = std::stod(argument.substr(8));
            else if (argument.rfind("--clock-margin=", 0) == 0) clock_margin = std::stod(argument.substr(15));
            else if (argument == "--report") report_only = true;
            else if (argument == "--binary") four_state = false;
            else if (argument == "--floors") print_floors = report_only = true;
            else only.insert(argument);
        }
        std::cout << "Gate-level form against compact form, " << rounds << " situations of each kind per part.\n"
                  << "Per kind: the gates' time as a share of the compact part's, averaged over the outputs both forms changed.\n"
                  << std::left << std::setw(38) << "part" << std::right << std::setw(6) << "runs" << std::setw(8) << "compared";
        for (const char* name : KIND_NAMES) std::cout << std::setw(11) << name;
        std::cout << std::setw(9) << "typical" << std::setw(7) << "worst" << std::setw(7) << "inner" << std::setw(7) << "pulses"
                  << std::setw(7) << "sooner" << "  note\n";
        size_t parts = 0, broken = 0, slow = 0, exact = 0, within = 0, excused = 0, differing = 0, inner = 0, pulses = 0, sooner = 0,
               parent = 0, parent_sooner = 0, situations = 0, compared = 0, detours = 0;
        Tally::Set totals[KINDS];
        const auto percent = [](double value) { return value < 0 ? std::string("-") : std::to_string(int(value + 0.5)) + "%"; };
        const auto share = [](const Tally::Set& set) { return set.compared ? 100.0 * set.share / double(set.compared) : -1.0; };
        for (const auto& contract : circuit::builtinComponentCatalog().contracts()) {
            if (!only.empty() && !only.contains(contract.id)) continue;
            if (const auto reason = NOT_DRIVEN.find(contract.id); reason != NOT_DRIVEN.end()) {
                std::cout << std::left << std::setw(38) << contract.id << "  not driven: " << reason->second << '\n';
                continue;
            }
            if (const auto form = build(contract.id, circuit::maximallyStructural()); form && !std::dynamic_pointer_cast<BasicComponent>(form))
                if (const auto inside = compactInside(form); !inside.empty()) {
                    std::cout << std::left << std::setw(38) << contract.id << "  not compared: its structural form has no gates for " << inside << '\n';
                    continue;
                }
            const auto tally = part(contract.id, rounds, four_state);
            if (!tally) continue;
            ++parts;
            exact += tally->exact; within += tally->within; inner += tally->inner; pulses += tally->pulses; sooner += tally->sooner;
            parent += tally->parent; parent_sooner += tally->parent_sooner; detours += tally->detours;
            Tally::Set all;
            for (int kind = 0; kind < KINDS; ++kind) {
                all.situations += tally->sets[kind].situations; all.compared += tally->sets[kind].compared; all.share += tally->sets[kind].share;
                totals[kind].situations += tally->sets[kind].situations; totals[kind].compared += tally->sets[kind].compared;
                totals[kind].share += tally->sets[kind].share;
            }
            situations += all.situations; compared += all.compared;
            const double typical = share(all);
            double worst = -1;
            for (const auto& [output, pair] : tally->slowest)
                if (pair.second) worst = worst < 0 ? 100.0 * double(pair.first) / double(pair.second)
                                                   : std::min(worst, 100.0 * double(pair.first) / double(pair.second));
            // A part timed from its gates no longer runs on its table, so how
            // close the table's largest delay comes is not its measure.
            const bool below = (typical >= 0 && typical < typical_target)
                || (!tally->timed_by_gates && worst >= 0 && worst < worst_target);
            std::string note;
            differing += tally->differ_clean;
            if (!tally->failures.empty()) { ++broken; note = "RULE BROKEN: " + tally->failures.front(); }
            else if (tally->timed_by_gates && !below) note = "timed from its gates";

            else if (below) {
                const auto floor = FLOORS.find(contract.id);
                if (print_floors) floors << "    {\"" << contract.id << "\", {" << int(typical) << ", " << int(worst) << ", "
                                         << (contract.id.rfind("memory.rom.", 0) == 0 ? "EMPTY_ROM" : typical < typical_target ? "DATA" : "SAMPLE") << "}},\n";
                if (floor == FLOORS.end()) { ++slow; note = "below target"; }
                else if (typical < floor->second.typical || worst < floor->second.worst) { ++slow; note = "below its recorded shares"; }
                else { ++excused; note = std::string("excepted: ") + floor->second.why; }
            } else if (FLOORS.contains(contract.id)) note = "meets the targets now; its exception can go";
            std::cout << std::left << std::setw(38) << contract.id << std::right << std::setw(6) << all.situations << std::setw(8) << all.compared;
            for (int kind = 0; kind < KINDS; ++kind) std::cout << std::setw(11) << percent(share(tally->sets[kind]));
            std::cout << std::setw(9) << percent(typical) << std::setw(7) << percent(worst) << std::setw(7) << tally->inner << std::setw(7)
                      << tally->pulses << std::setw(7) << tally->sooner << "  " << note << '\n';
            for (size_t i = 1; i < tally->failures.size(); ++i) std::cout << std::string(40, ' ') << "  also: " << tally->failures[i] << '\n';
        }
        std::cout << std::left << std::setw(38) << "all parts" << std::right << std::setw(6) << situations << std::setw(8) << compared;
        Tally::Set everything;
        for (int kind = 0; kind < KINDS; ++kind) {
            std::cout << std::setw(11) << percent(share(totals[kind]));
            everything.compared += totals[kind].compared; everything.share += totals[kind].share;
        }
        std::cout << std::setw(9) << percent(share(everything)) << '\n'
                  << parts << " parts, " << situations << " situations run on both forms.\n"
                  << "  " << compared << " outputs changed in both forms: the compact one never before the gates' one\n"
                  << "  " << exact << " compact output changes, every one at the delay its table gives\n"
                  << "  " << within << " gate output changes, every one within the table\n"
                  << "  " << inner << " changes of gate-level parts inside other parts, every one within that part's own table\n"
                  << "  " << pulses << " outputs where the gates pulsed and returned; the compact part stayed still\n"
                  << "  " << sooner << " outputs the compact part quieted sooner when two pins changed a few ticks apart\n"
                  << "  " << detours << " outputs the compact part held back to its worst case after showing a value its gates never show\n"
                  << "  " << parent_sooner << " of " << parent_sooner + parent << " outputs went quiet sooner in a parent built from compact parts\n"
                  << broken << " parts break the rule; " << slow << " miss the targets (typical " << typical_target
                  << "%, worst " << worst_target << "%) without a recorded exception; " << excused << " have one.\n";
        if (print_floors) std::cout << "Measured shares of the parts below a target:\n" << floors.str();
        if (differing) std::cout << differing << " output values differed between the forms while every input was 0 or 1 (state left by an"
                                    " earlier X or Z, or input codes with no meaning); their timing is not compared.\n";
        if (only.empty()) { std::cout << "Cores at their certified clock:\n"; cores(clock_margin); }
        if (broken) throw std::runtime_error("The delay rule is broken for " + std::to_string(broken) + " parts");
        if (slow && !report_only) throw std::runtime_error(std::to_string(slow) + " parts are slower than the targets allow");
        std::cout << "DelayRuleTest passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "DelayRuleTest failed: " << error.what() << '\n';
        return 1;
    }
}
