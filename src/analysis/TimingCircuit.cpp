#include "TimingCircuit.hpp"
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/DFlipFlopTiming.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "modules/storage/ClockedStateBitBehavioral.hpp"
#include "modules/storage/RegisterBehavioral.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "../modules/memory/FlipFlopCellBehavioral.hpp"
#include "modules/memory/DigitalMemory.hpp"
#include "modules/memory/DRAM.hpp"
#include "modules/memory/SRAM.hpp"
#include "modules/memory/SRAMCellTiming.hpp"
#include "../modules/rv32i/RegisterFile32x32Behavioral.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <typeinfo>

namespace circuit::analysis::detail {
namespace {

template<size_t W>
bool utilityContract(const IOComponent& c, ClockComponentContract& m) {
    if (typeid(c) == typeid(ConstantValue<W>)) {
        m.constant_outputs["OUT"] = static_cast<const ConstantValue<W>&>(c).getConstantValue()
            & ((uint64_t{1} << W) - 1);
        return true;
    }
    if (typeid(c) != typeid(BitSplitter<W>) && typeid(c) != typeid(BitJoiner<W>)) return false;
    const auto delay = static_cast<const BasicComponent&>(c).getDelay();
    for (const auto& [i, p] : c.getAllInputPins())
        for (const auto& [o, q] : c.getAllOutputPins()) m.arcs.push_back({i, o, {delay, delay}});
    return true;
}
template<size_t W> bool splitter(const IOComponent& c) { return typeid(c) == typeid(BitSplitter<W>); }
template<size_t W> bool joiner(const IOComponent& c) { return typeid(c) == typeid(BitJoiner<W>); }
} // namespace

bool isBitSplitter(const IOComponent& c) {
    return splitter<1>(c)||splitter<2>(c)||splitter<3>(c)||splitter<4>(c)
        ||splitter<5>(c)||splitter<8>(c)||splitter<16>(c)||splitter<32>(c);
}
bool isBitJoiner(const IOComponent& c) {
    return joiner<1>(c)||joiner<2>(c)||joiner<3>(c)||joiner<4>(c)
        ||joiner<5>(c)||joiner<8>(c)||joiner<16>(c)||joiner<32>(c);
}

std::optional<ClockComponentContract> builtinClockContract(const IOComponent& c) {
    // The gate-level flip-flop is a loop of latches; no path analysis bounds
    // it. Its contract is measured exhaustively (DFlipFlopTimingTest) and it is
    // then one indivisible part, like any other with a contract.
    if (typeid(c) == typeid(DFlipFlop)) {
        using namespace circuit::dflipflop_timing;
        ClockComponentContract flop;
        flop.evidence = "gate-level DFlipFlop, every binary case tried (DFlipFlopTimingTest); reset held LOW";
        flop.clock_pin = "CLK";
        flop.fixed_inputs["RST"] = 0;
        flop.captures = {{"D", ClockEdge::Rising, SETUP, HOLD}};
        flop.launches = {{"Q", ClockEdge::Rising, {CLOCK_TO_OUTPUT_MINIMUM, CLOCK_TO_OUTPUT_MAXIMUM}},
                         {"Q_BAR", ClockEdge::Rising, {CLOCK_TO_OUTPUT_MINIMUM, CLOCK_TO_OUTPUT_MAXIMUM}}};
        flop.minimum_high_ticks = MINIMUM_HIGH;
        flop.minimum_low_ticks = MINIMUM_LOW;
        return flop;
    }
    // The gate-level SRAM cell keeps its bit in a latch, also a loop. Every
    // case is tried (SRAMCellTimingTest): each input reaches the outputs in
    // 2 to 3 ticks, provided input changes are at least 2 ticks apart.
    if (typeid(c) == typeid(SRAMCell)) {
        using namespace circuit::sram_cell_timing;
        ClockComponentContract cell;
        cell.evidence = "gate-level SRAMCell, every case tried (SRAMCellTimingTest); input changes at least "
            + std::to_string(MINIMUM_SPACING) + " ticks apart";
        for (const auto& [i, p] : c.getAllInputPins())
            for (const auto& [o, q] : c.getAllOutputPins()) cell.arcs.push_back({i, o, {EARLIEST, LATEST}});
        return cell;
    }
    // Built-in contracts cover concrete primitive instances, not an arbitrarily
    // modified hierarchy attached beneath one of those instances.
    if (!c.getChildren().empty()) return std::nullopt;
    // A part that declares its delays: the table the simulator runs on is
    // also its contract.
    if (const auto* part = dynamic_cast<const BasicComponent*>(&c)) if (const auto* table = part->getDelays()) {
        ClockComponentContract declared;
        declared.evidence = "declared part delays: " + table->evidence;
        declared.clock_pin = table->clock;
        declared.fixed_inputs = table->fixed_inputs;
        std::set<std::string> read{table->clock}, written;
        for (const auto& [name, value] : table->fixed_inputs) read.insert(name);
        for (const auto& arc : table->arcs) {
            declared.arcs.push_back({arc.input, arc.output, {arc.delay.minimum, arc.delay.maximum}});
            read.insert(arc.input); written.insert(arc.output);
        }
        for (const auto& [output, range] : table->launches) {
            declared.launches.push_back({output, ClockEdge::Rising, {range.minimum, range.maximum}});
            written.insert(output);
        }
        for (const auto& [input, setup] : table->setup) {
            declared.captures.push_back({input, ClockEdge::Rising, setup, 0});
            read.insert(input);
        }
        // The longest path between its own registers needs this much clock.
        if (!table->clock.empty())
            declared.minimum_high_ticks = declared.minimum_low_ticks = std::max<size_t>(1, (table->internal_period + 2) / 2);
        declared.clock_outputs = table->clock_outputs;
        declared.self_timed = table->self_timed;
        for (const auto& output : table->clock_outputs) written.insert(output);
        for (const auto& [name, pin] : c.getAllInputPins()) if (!read.contains(name)) declared.unused_inputs.push_back(name);
        for (const auto& [name, pin] : c.getAllOutputPins()) if (!written.contains(name)) declared.stable_outputs.push_back(name);
        return declared;
    }
    ClockComponentContract m;
    m.evidence = "simulator-exact-primitives-v1";
    // The memory sequencer: clocked, and it also wakes itself to step through
    // device phases. Every output may change one delay after either.
    if (const auto* primitive = dynamic_cast<const circuit::memory_device::Primitive*>(&c);
        primitive && primitive->selfTimedClockedPart() && c.getInputPinDynamic("CLK")) {
        const auto d = primitive->getDelay();
        m.clock_pin = "CLK";
        m.self_timed = true;
        for (const auto& [i, p] : c.getAllInputPins()) if (i != "CLK") m.captures.push_back({i, ClockEdge::Rising, 0, 0});
        for (const auto& [o, q] : c.getAllOutputPins()) m.launches.push_back({o, ClockEdge::Rising, {d, d}});
        return m;
    }
    // The SRAM bit-line pair. What it sends back to the cells (CELL_DRIVE,
    // CELL_BL, CELL_BL_BAR) is set from the precharge, drive, write data and
    // word lines only (BitlineState::update); the cells' own outputs reach
    // just the read side. Saying so keeps the write path from looking like a
    // loop through the cells.
    if (typeid(c) == typeid(SRAMBitlinePair)) {
        const auto d = static_cast<const BasicComponent&>(c).getDelay();
        for (const auto& [i, p] : c.getAllInputPins())
            for (const auto& [o, q] : c.getAllOutputPins()) {
                const bool from_cell = i.rfind("Q_", 0) == 0, to_cell = o.rfind("CELL_", 0) == 0;
                if (!(from_cell && to_cell)) m.arcs.push_back({i, o, {d, d}});
            }
        return m;
    }
    // The DRAM cell and its sense column drive each other: the column reads
    // the cell's sample and writes the value back. It is not a loop, because
    // of what each output depends on (DRAMCell::evaluate and
    // DRAMSenseColumn::evaluate): the cell's SAMPLE follows the word line
    // only, never RESTORE or D, and the column's RESTORE follows precharge,
    // sense and the word lines only, never a sample or the write data. Both
    // also step on their own schedule.
    if (typeid(c) == typeid(DRAMCell) || typeid(c) == typeid(DRAMSenseColumn)) {
        const bool cell = typeid(c) == typeid(DRAMCell);
        const auto d = static_cast<const BasicComponent&>(c).getDelay();
        m.self_timed = true;
        for (const auto& [i, p] : c.getAllInputPins())
            for (const auto& [o, q] : c.getAllOutputPins()) {
                const bool absent = cell ? (o == "SAMPLE" && i != "WL")
                    : (o == "RESTORE" && (i == "WRITE" || i == "D" || i.rfind("SAMPLE_", 0) == 0));
                if (!absent) m.arcs.push_back({i, o, {d, d}});
            }
        return m;
    }
    // A primitive that is not made of gates and publishes only while being
    // evaluated: every input can reach every output in its one delay,
    // whatever it remembers inside. One that also wakes itself is self-timed:
    // the delay then counts from whatever caused the change.
    if (const auto* primitive = dynamic_cast<const BasicComponent*>(&c);
        primitive && primitive->primitiveTiming() != BasicComponent::PrimitiveTiming::Unknown
        && !c.getAllInputPins().empty()) {
        const auto d = primitive->getDelay();
        m.self_timed = primitive->primitiveTiming() == BasicComponent::PrimitiveTiming::SelfTimed;
        const auto* memory = dynamic_cast<const circuit::memory_device::Primitive*>(primitive);
        for (const auto& [i, p] : c.getAllInputPins())
            for (const auto& [o, q] : c.getAllOutputPins())
                if (!memory || memory->reaches(i, o)) m.arcs.push_back({i, o, {d, d}});
        return m;
    }
    if (typeid(c) == typeid(NOTGate) || typeid(c) == typeid(ANDGate)
        || typeid(c) == typeid(NANDGate) || typeid(c) == typeid(ORGate)
        || typeid(c) == typeid(NORGate) || typeid(c) == typeid(XORGate)) {
        const auto d = static_cast<const BasicComponent&>(c).getDelay();
        for (const auto& [i, p] : c.getAllInputPins())
            for (const auto& [o, q] : c.getAllOutputPins()) m.arcs.push_back({i, o, {d, d}});
        return m;
    }
    if (utilityContract<1>(c, m) || utilityContract<2>(c, m) || utilityContract<3>(c, m)
        || utilityContract<4>(c, m) || utilityContract<5>(c, m) || utilityContract<8>(c, m)
        || utilityContract<16>(c, m) || utilityContract<32>(c, m)) return m;
    if (typeid(c) == typeid(Rewire)) {
        const auto& rw = static_cast<const Rewire&>(c);
        std::set<std::pair<std::string, std::string>> arcs;
        std::set<std::string> used, produced;
        for (const auto& bit : rw.getBitMappings()) {
            arcs.emplace(bit.src_wire, bit.dst_wire);
            used.insert(bit.src_wire); produced.insert(bit.dst_wire);
        }
        for (const auto& [i, o] : arcs) m.arcs.push_back({i, o, {0, 0}});
        for (const auto& [i, p] : c.getAllInputPins()) if (!used.contains(i)) m.unused_inputs.push_back(i);
        for (const auto& [o, p] : c.getAllOutputPins()) if (!produced.contains(o)) {
            if (rw.getUnmappedDefault() == Rewire::UnmappedBitValue::UNKNOWN) m.stable_outputs.push_back(o);
            else m.constant_outputs[o] = rw.getUnmappedDefault() == Rewire::UnmappedBitValue::LOW
                ? 0 : (uint64_t{1} << p->getWidth()) - 1;
        }
        return m;
    }
    // Exact concrete types only: derived endpoints can override their behavior.
    // Delayed technologies schedule autonomous wakeups and need a different
    // timing model; the FF contract must never be selected by port shape alone.
    if (typeid(c) == typeid(BehavioralMemoryEndpoint)
        || typeid(c) == typeid(CompactMemoryEndpoint<8, circuit::memory::FFModel>)
        || typeid(c) == typeid(CompactMemoryEndpoint<16, circuit::memory::FFModel>)
        || typeid(c) == typeid(CompactMemoryEndpoint<32, circuit::memory::FFModel>)) {
        const auto& endpoint = static_cast<const BehavioralMemoryEndpoint&>(c);
        if (endpoint.technology() != circuit::memory::Technology::FF) return std::nullopt;
        m.evidence = "simulator-FF-endpoint-all-state-v1; MemoryEndpointSafeClockTest; reset held LOW after initialization";
        m.clock_pin = "CLK"; m.fixed_inputs["RST"] = 0;
        m.minimum_high_ticks = m.minimum_low_ticks = 1;
        const auto d = endpoint.getDelay();
        // The rising edge commits writes and updates FULL / saved DATA / STATUS.
        // Every non-reset request/control input participates in one of those
        // updates. Captures and launches are a conservative union over all
        // empty, held, consume-and-replace, read, write and error states.
        for (auto i : {"REQ_VALID", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK", "RSP_READY"})
            m.captures.push_back({i, ClockEdge::Rising, 0, 0});
        for (auto o : {"REQ_READY", "RSP_VALID", "RSP_READ_DATA", "RSP_STATUS"})
            m.launches.push_back({o, ClockEdge::Rising, {d, d}});
        // With reset LOW and state held fixed, evaluate() simplifies to:
        // READY = !full || RSP_READY; VALID = full || REQ_VALID;
        // DATA = full ? saved_data : (REQ_VALID && legal_read ? mem[addr] : 0);
        // STATUS = full ? saved_status : validate(op, addr, mask).
        // Thus RSP_READY is NOT a combinational dependency of VALID or DATA;
        // nor are write data/mask dependencies of read DATA. Do not import
        // state-specific cuts from the legacy core timing estimator.
        for (const auto& arc : std::vector<std::pair<std::string, std::string>>{
                {"RSP_READY", "REQ_READY"}, {"REQ_VALID", "RSP_VALID"},
                {"REQ_VALID", "RSP_READ_DATA"}, {"REQ_OP", "RSP_READ_DATA"},
                {"REQ_ADDR", "RSP_READ_DATA"}, {"REQ_OP", "RSP_STATUS"},
                {"REQ_ADDR", "RSP_STATUS"}, {"REQ_WRITE_MASK", "RSP_STATUS"}})
            m.arcs.push_back({arc.first, arc.second, {d, d}});
        return m;
    }
    // Match concrete implementation types, not names/metadata: a structural
    // master/slave DFF is NOT given an invented one-tick register contract.
    const bool reg = typeid(c) == typeid(RegisterBehavioral) || typeid(c) == typeid(FlipFlopCellBehavioral)
        || typeid(c) == typeid(ClockedStateBitBehavioral);
    const bool rf = typeid(c) == typeid(RegisterFile32x32Behavioral);
    if (reg || rf) {
        m.evidence = "simulator-direct-FF-storage-v1; reset held LOW after initialization";
        m.clock_pin = "CLK"; m.fixed_inputs["RST"] = 0;
        m.minimum_high_ticks = m.minimum_low_ticks = 1;
        for (const auto& [i, p] : c.getAllInputPins()) {
            if (i == "CLK" || i == "RST" || (rf && (i == "RS1_ADDR" || i == "RS2_ADDR"))) continue;
            m.captures.push_back({i, ClockEdge::Rising, 0, 0});
        }
        const auto d = static_cast<const BasicComponent&>(c).getDelay();
        for (const auto& [o, p] : c.getAllOutputPins()) m.launches.push_back({o, ClockEdge::Rising, {d, d}});
        if (rf) m.arcs = {{"RS1_ADDR", "RS1_DATA", {d, d}}, {"RS2_ADDR", "RS2_DATA", {d, d}}};
        return m;
    }
    return std::nullopt;
}

void collectTimingLeaves(const std::shared_ptr<Component>& root, const TimingContracts& contracts,
                         size_t maximum_depth, std::vector<TimingLeaf>& leaves,
                         const TimingLeafVisitor& visitor) {
    const auto visit = [&](auto&& self, const std::shared_ptr<Component>& component, size_t depth) -> void {
        if (depth >= maximum_depth) throw std::runtime_error("Timing hierarchy depth budget exhausted");
        if (auto io = std::dynamic_pointer_cast<IOComponent>(component)) {
            if (visitor.component) visitor.component(io);
            auto contract = contracts.model ? contracts.model(*io) : std::nullopt;
            const bool builtin_rules = !contract;
            if (!contract) contract = builtinClockContract(*io);
            if (contract) {
                leaves.push_back({io, std::move(*contract), builtin_rules});
                return;
            }
        }
        if (std::dynamic_pointer_cast<BasicComponent>(component) || component->getChildren().empty()) {
            if (visitor.uncovered) visitor.uncovered(*component);
            return;
        }
        for (const auto& child : component->getChildren()) self(self, child, depth + 1);
    };
    visit(visit, root, 0);
}

std::shared_ptr<PinBase> drivingPin(std::shared_ptr<PinBase> pin,
                                    const std::unordered_set<const PinBase*>& sources) {
    std::unordered_set<const PinBase*> seen;
    while (pin) {
        if (!seen.insert(pin.get()).second) throw std::runtime_error("Boundary wire cycle: " + pin->getID());
        if (sources.contains(pin.get())) return pin;
        auto wire = pin->getType() == PinType::INPUT ? pin->getExternalWireBase() : pin->getInternalWireBase();
        // Same-owner BasicComponent feedback is stored as an internal wire.
        // A composite input's ordinary internal wire, conversely, starts at
        // that very pin and is not an upstream driver.
        if (!wire && pin->getType() == PinType::INPUT) {
            const auto internal = pin->getInternalWireBase();
            if (internal && internal->getSourcePinBase() != pin) wire = internal;
        }
        if (!wire || !wire->getSourcePinBase()) return pin;
        pin = wire->getSourcePinBase();
    }
    throw std::runtime_error("Missing wire source");
}

} // namespace circuit::analysis::detail
