#include "simulator/MachineState.hpp"

#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "simulator/Event.hpp"
#include "simulator/EventPool.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <unordered_map>

namespace circuit::state {
namespace {

constexpr uint32_t MachineTag = 0x4d414348;  // "MACH"
constexpr uint32_t QueueTag = 0x51554555;    // "QUEU"
constexpr uint32_t WireTag = 0x57495245;     // "WIRE"
constexpr uint32_t PinTag = 0x50494e53;      // "PINS"
constexpr uint32_t PartTag = 0x50415254;     // "PART"
constexpr uint32_t EndTag = 0x454e4421;      // "END!"
constexpr uint32_t Version = 2;

struct Numbering {
    std::vector<std::shared_ptr<Component>> components;
    std::vector<std::shared_ptr<PinBase>> pins;
    std::vector<std::shared_ptr<WireBase>> wires;
    std::unordered_map<const Component*, uint32_t> component_number;
    std::unordered_map<const PinBase*, uint32_t> pin_number;
    std::unordered_map<const WireBase*, uint32_t> wire_number;
    uint64_t hash = 1469598103934665603ull;

    explicit Numbering(const std::shared_ptr<Component>& root) { visit(root); }

    void mix(const std::string& text) {
        for (unsigned char c : text) { hash ^= c; hash *= 1099511628211ull; }
        hash ^= 0xff; hash *= 1099511628211ull;
    }
    void addWire(const std::shared_ptr<WireBase>& wire) {
        if (!wire || !wire_number.emplace(wire.get(), static_cast<uint32_t>(wires.size())).second) return;
        wires.push_back(wire);
        mix(std::to_string(wire->getWidth()));
    }
    void visit(const std::shared_ptr<Component>& component) {
        component_number.emplace(component.get(), static_cast<uint32_t>(components.size()));
        components.push_back(component);
        mix(component->getName());
        mix(component->getTypeName());
        for (const auto& wire : component->getAllWires()) addWire(wire);
        if (const auto io = std::dynamic_pointer_cast<IOComponent>(component)) {
            for (const auto* pins_by_name : {&io->getAllInputPins(), &io->getAllOutputPins()})
                for (const auto& [name, pin] : *pins_by_name) {
                    pin_number.emplace(pin.get(), static_cast<uint32_t>(pins.size()));
                    pins.push_back(pin);
                    mix(name);
                    mix(std::to_string(pin->getWidth()));
                    addWire(pin->getExternalWireBase());
                    addWire(pin->getInternalWireBase());
                }
        }
        for (const auto& child : component->getChildren()) visit(child);
    }
    template <class Map, class Key> uint32_t number(const Map& map, const Key* key, const char* what) const {
        const auto found = map.find(key);
        if (found == map.end()) throw std::logic_error(std::string("A queued event names a ") + what + " outside the saved tree");
        return found->second;
    }
};

void putCounters(Writer& out, const SimulatorPerformanceCounters& c) {
    for (uint64_t v : {c.scheduled_events, c.scheduled_wire_updates, c.scheduled_component_evaluations,
                       c.processed_events, c.processed_wire_updates, c.processed_component_evaluations,
                       c.effective_wire_changes, c.effective_pin_changes})
        out.u64(v);
    out.size(c.maximum_event_queue_depth);
}

SimulatorPerformanceCounters getCounters(Reader& in) {
    SimulatorPerformanceCounters c;
    for (uint64_t* v : {&c.scheduled_events, &c.scheduled_wire_updates, &c.scheduled_component_evaluations,
                        &c.processed_events, &c.processed_wire_updates, &c.processed_component_evaluations,
                        &c.effective_wire_changes, &c.effective_pin_changes})
        *v = in.u64();
    c.maximum_event_queue_depth = static_cast<size_t>(in.u64());
    return c;
}

// Every value as `width` value bits in one stream, then the few values that
// hold unknown or floating bits. Most of a machine's values are known bits.
template <class Handles> void putValues(Writer& out, const Handles& handles) {
    std::vector<uint8_t> bits;
    uint64_t at = 0;
    std::vector<std::pair<uint32_t, uint64_t>> unknown;
    for (size_t i = 0; i < handles.size(); ++i) {
        const auto& value = handles[i]->packed();
        const size_t width = handles[i]->getWidth();
        for (size_t bit = 0; bit < width; ++bit, ++at) {
            if (at % 8 == 0) bits.push_back(0);
            if ((value.value >> bit) & 1) bits.back() |= uint8_t(1u << (at % 8));
        }
        if (value.unknown) unknown.push_back({static_cast<uint32_t>(i), value.unknown});
    }
    out.bytes(bits);
    out.size(unknown.size());
    for (const auto& [index, mask] : unknown) { out.u32(index); out.u64(mask); }
}
template <class Handles> void getValues(Reader& in, const Handles& handles) {
    const auto bits = in.bytes();
    uint64_t at = 0;
    std::vector<PackedLogic> values(handles.size());
    for (size_t i = 0; i < handles.size(); ++i) {
        const size_t width = handles[i]->getWidth();
        if ((at + width + 7) / 8 > bits.size()) in.fail("value bits end early");
        for (size_t bit = 0; bit < width; ++bit, ++at)
            if ((bits[at / 8] >> (at % 8)) & 1) values[i].value |= uint64_t{1} << bit;
    }
    if ((at + 7) / 8 != bits.size()) in.fail("value bits of another length");
    for (size_t i = 0, n = in.size(); i < n; ++i) {
        const auto index = in.u32();
        if (index >= handles.size()) in.fail("an unknown value outside the machine");
        values[index].unknown = in.u64();
    }
    for (size_t i = 0; i < handles.size(); ++i) handles[i]->setPacked(values[i]);
}

} // namespace

std::vector<std::string> undeclaredStateTypes(const std::shared_ptr<Component>& root) {
    std::set<std::string> types;
    for (const auto& component : Numbering(root).components)
        if (const auto part = std::dynamic_pointer_cast<BasicComponent>(component))
            if (part->stateSupport() == BasicComponent::StateSupport::Unknown) types.insert(part->getTypeName());
    return {types.begin(), types.end()};
}

void saveMachine(const std::shared_ptr<Component>& root, const Simulator& sim, Writer& out) {
    const Numbering n(root);
    if (const auto missing = undeclaredStateTypes(root); !missing.empty()) {
        std::string names;
        for (const auto& name : missing) names += (names.empty() ? "" : ", ") + name;
        throw std::logic_error("Cannot save a machine with parts that have not declared their state: " + names);
    }
    out.tag(MachineTag);
    out.u32(Version);
    out.size(n.components.size());
    out.size(n.pins.size());
    out.size(n.wires.size());
    out.u64(n.hash);

    out.tag(QueueTag);
    out.time(sim.getCurrentTime());
    out.u64(sim.nextSchedulingOrder());
    putCounters(out, sim.getPerformanceCounters());
    const auto events = sim.queuedEvents();
    std::unordered_map<const Event*, int64_t> event_numbers;
    out.size(events.size());
    for (size_t i = 0; i < events.size(); ++i) {
        const auto& record = events[i];
        event_numbers.emplace(record.event.get(), static_cast<int64_t>(i));
        out.time(record.time);
        out.u64(record.order);
        if (const auto update = std::dynamic_pointer_cast<WireUpdateEvent>(record.event)) {
            out.u8(0);
            out.u32(n.number(n.wire_number, update->wire_to_update.get(), "wire"));
            out.packed(update->new_value);
        } else if (const auto evaluation = std::dynamic_pointer_cast<ComponentEvalEvent>(record.event)) {
            out.u8(1);
            out.u32(n.number(n.component_number, evaluation->component_to_evaluate.get(), "component"));
        } else {
            throw std::logic_error("Cannot save a queued event of an unknown kind");
        }
    }
    auto headless = sim.headlessPinStates();
    std::vector<std::pair<uint32_t, const Simulator::HeadlessPinState*>> ordered;
    for (const auto& state : headless) ordered.push_back({n.number(n.pin_number, state.first.get(), "pin"), &state});
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    out.size(ordered.size());
    for (const auto& [number, state] : ordered) {
        out.u32(number);
        out.time(state->second.first);
        out.logic(state->second.second);
    }

    out.tag(WireTag);
    putValues(out, n.wires);
    out.tag(PinTag);
    putValues(out, n.pins);

    out.tag(PartTag);
    const auto number_of = [&](const Event* event) -> int64_t {
        const auto found = event_numbers.find(event);
        return found == event_numbers.end() ? -1 : found->second;
    };
    for (size_t i = 0; i < n.components.size(); ++i) {
        const auto part = std::dynamic_pointer_cast<BasicComponent>(n.components[i]);
        if (!part) continue;
        const auto before = out.data().size();
        part->saveTimingState(out, number_of);
        const auto timing = out.data().size() - before;
        if (part->stateSupport() == BasicComponent::StateSupport::Saved) part->saveOwnState(out);
        if (std::getenv("CIRCUITSIM_SAVE_SIZES") && out.data().size() - before > 2000)
            std::fprintf(stderr, "save %zu bytes (timing %zu): %s %s\n", out.data().size() - before, timing, part->getTypeName(), part->getID().c_str());
    }
    out.tag(EndTag);
}

void restoreMachine(const std::shared_ptr<Component>& root, Simulator& sim, Reader& in) {
    const Numbering n(root);
    in.tag(MachineTag, "machine");
    if (in.u32() != Version) in.fail("an unsupported save version");
    const auto components = in.u64(), pins = in.u64(), wires = in.u64();
    if (components != n.components.size() || pins != n.pins.size() || wires != n.wires.size())
        in.fail("a machine with " + std::to_string(components) + " components, " + std::to_string(pins) + " pins and " +
                std::to_string(wires) + " wires; this one has " + std::to_string(n.components.size()) + ", " +
                std::to_string(n.pins.size()) + " and " + std::to_string(n.wires.size()));
    if (in.u64() != n.hash) in.fail("a machine with other names, types or widths");

    in.tag(QueueTag, "event queue");
    const auto now = in.time();
    const auto next_order = in.u64();
    const auto counters = getCounters(in);
    std::vector<Simulator::QueuedEventRecord> records(in.size());
    std::vector<std::shared_ptr<Event>> events;
    events.reserve(records.size());
    for (auto& record : records) {
        record.time = in.time();
        record.order = in.u64();
        const auto kind = in.u8();
        if (kind == 0) {
            const auto wire = in.u32();
            if (wire >= n.wires.size()) in.fail("an event on a missing wire");
            const auto value = in.packed();
            record.event = circuit::makeEvent<WireUpdateEvent>(record.time, n.wires[wire], value);
        } else if (kind == 1) {
            const auto component = in.u32();
            if (component >= n.components.size()) in.fail("an event for a missing component");
            record.event = circuit::makeEvent<ComponentEvalEvent>(record.time, n.components[component]);
        } else {
            in.fail("an event of an unknown kind");
        }
        record.priority = record.event->getPriority();
        events.push_back(record.event);
    }
    std::vector<Simulator::HeadlessPinState> headless(in.size());
    for (auto& [pin, state] : headless) {
        const auto number = in.u32();
        if (number >= n.pins.size()) in.fail("a missing pin");
        pin = n.pins[number];
        state.first = in.time();
        state.second = in.logicList();
    }

    in.tag(WireTag, "wire value");
    getValues(in, n.wires);
    in.tag(PinTag, "pin value");
    getValues(in, n.pins);

    in.tag(PartTag, "part state");
    const auto event_at = [&](int64_t number) -> std::shared_ptr<Event> {
        if (number < 0 || static_cast<size_t>(number) >= events.size()) in.fail("a pending change naming a missing event");
        return events[static_cast<size_t>(number)];
    };
    for (size_t i = 0; i < n.components.size(); ++i) {
        const auto part = std::dynamic_pointer_cast<BasicComponent>(n.components[i]);
        if (!part) continue;
        if (part->stateSupport() == BasicComponent::StateSupport::Unknown)
            throw std::logic_error("Cannot restore " + part->getID() + " (" + part->getTypeName() + "): its state is not declared");
        part->restoreTimingState(in, event_at);
        if (part->stateSupport() == BasicComponent::StateSupport::Saved) part->restoreOwnState(in);
    }
    in.tag(EndTag, "end");

    sim.restoreScheduling(now, next_order, std::move(records), counters, headless);
    // A recording run keeps history from here on: every value starts at the
    // restored one.
    if (sim.isHistoryRecordingEnabled()) sim.seedHistory(now, n.wires, headless);
    // Parts that keep a viewing history restart it at the restored state.
    for (const auto& component : n.components)
        if (const auto part = std::dynamic_pointer_cast<BasicComponent>(component)) part->stateRestored(now, sim);
}

void restoreValues(const std::shared_ptr<Component>& root, Simulator& sim, Reader& in) {
    const Numbering n(root);
    in.tag(MachineTag, "machine");
    if (in.u32() != Version) in.fail("an unsupported save version");
    if (in.u64() != n.components.size() || in.u64() != n.pins.size() || in.u64() != n.wires.size() || in.u64() != n.hash)
        in.fail("another machine");
    in.tag(QueueTag, "event queue");
    const auto now = in.time();
    in.u64();
    (void)getCounters(in);
    for (size_t i = 0, count = in.size(); i < count; ++i) {
        in.time(); in.u64();
        const auto kind = in.u8();
        in.u32();
        if (kind == 0) in.packed();
        else if (kind != 1) in.fail("an event of an unknown kind");
    }
    for (size_t i = 0, count = in.size(); i < count; ++i) { in.u32(); in.time(); in.logicList(); }
    in.tag(WireTag, "wire value");
    getValues(in, n.wires);
    in.tag(PinTag, "pin value");
    getValues(in, n.pins);
    sim.restoreTime(now);
}

} // namespace circuit::state
