#include "TimingCircuit.hpp"
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "modules/storage/ClockedStateBitBehavioral.hpp"
#include "modules/storage/RegisterBehavioral.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "../modules/memory/FlipFlopCellBehavioral.hpp"
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
    // Built-in contracts cover concrete primitive instances, not an arbitrarily
    // modified hierarchy attached beneath one of those instances.
    if (!c.getChildren().empty()) return std::nullopt;
    ClockComponentContract m;
    m.evidence = "simulator-exact-primitives-v1";
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
