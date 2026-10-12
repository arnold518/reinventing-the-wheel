#include "analysis/DelayTwin.hpp"
#include "TimingGraph.hpp"
#include "components/IOComponent.hpp"
#include "basic/PinBase.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/DFlipFlopTiming.hpp"
#include <algorithm>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>

namespace circuit::analysis {
namespace {
using namespace detail::timing;

// Values while timing: 0, 1, 2 for X or Z, and 3 for a bit this list cannot
// work out because it comes from a leaf that is not a gate. Such a bit is
// timed by that leaf's own delays; only its value is left open.
constexpr uint8_t UNKNOWN = 2, OPAQUE = 3;
constexpr size_t LARGEST_TWIN = 60000;
// A part with state walks its gates on every clock edge; whole cores stay on their tables.
constexpr size_t LARGEST_WITH_STATE = 12000;

class GateTwin final : public circuit::DelayTwin {
public:
    struct Gate { Kind kind; int a, b; Tick delay; uint8_t constant; };
    // A flip-flop: where its data and reset arrive and its outputs leave.
    struct Flop { int d = -1, reset = -1, q = -1, q_bar = -1; Tick launch = 0; };
    struct Memory final : State {
        std::vector<uint8_t> value, held;   // every node after the last change; the bit of each flip-flop
        std::vector<Tick> time, held_since;
        uint8_t clock = UNKNOWN;
        bool started = false;
    };
    std::vector<Gate> gates;                 // by node
    std::vector<int> order;                  // gates that are computed, inputs first
    std::vector<std::vector<int>> input_nodes, output_nodes;
    std::vector<Flop> flops;
    // A leaf that is not a gate: its output follows each listed input by that delay. Gate::a is its index.
    std::vector<std::vector<std::pair<int, Tick>>> leaves;
    int clock_input = -1;

    std::unique_ptr<State> start() const override {
        if (flops.empty()) return nullptr;
        auto memory = std::make_unique<Memory>();
        memory->held.assign(flops.size(), UNKNOWN);
        memory->held_since.assign(flops.size(), NEVER);
        return memory;
    }

    bool settle(State* state, long long now, const std::vector<std::vector<LogicValue>>& values,
                const std::vector<std::vector<long long>>& changed, std::vector<long long>& settled,
                std::vector<std::vector<uint8_t>>& result) const override {
        thread_local std::vector<uint8_t> value;
        thread_local std::vector<Tick> time;
        const auto code = [](LogicValue level) { return uint8_t(level == LogicValue::LOW ? 0 : level == LogicValue::HIGH ? 1 : UNKNOWN); };
        auto* memory = static_cast<Memory*>(state);
        bool answered = true;
        if (memory) {
            const uint8_t clock = clock_input >= 0 ? code(values[clock_input][0]) : UNKNOWN;
            if (memory->started && clock == UNKNOWN && memory->clock != UNKNOWN) {
                // An unknown clock may or may not have been an edge.
                for (size_t flop = 0; flop < flops.size(); ++flop) if (memory->held[flop] != UNKNOWN) {
                    memory->held[flop] = UNKNOWN;
                    memory->held_since[flop] = now + flops[flop].launch;
                }
                answered = false;
            }
            if (memory->started && memory->clock == 0 && clock == 1) for (size_t flop = 0; flop < flops.size(); ++flop) {
                // The edge takes what stood at the data input before this
                // change, if it had been quiet for the setup time.
                const auto& f = flops[flop];
                const uint8_t reset = f.reset >= 0 ? memory->value[f.reset] : 0;
                const bool quiet = memory->time[f.d] == NEVER || memory->time[f.d] + Tick(circuit::dflipflop_timing::SETUP) <= now;
                const uint8_t next = reset == 0 ? (quiet ? std::min(memory->value[f.d], UNKNOWN) : UNKNOWN) : reset == 1 ? 0 : UNKNOWN;
                if (next != memory->held[flop]) { memory->held[flop] = next; memory->held_since[flop] = now + f.launch; }
            }
            memory->clock = clock;
        }
        value.resize(gates.size());
        time.resize(gates.size());
        for (size_t node = 0; node < gates.size(); ++node) { value[node] = gates[node].constant; time[node] = NEVER; }
        for (size_t pin = 0; pin < input_nodes.size(); ++pin) for (size_t bit = 0; bit < input_nodes[pin].size(); ++bit) {
            const int node = input_nodes[pin][bit];
            if (node < 0) continue;
            value[node] = code(values[pin][bit]);
            time[node] = changed[pin][bit];
        }
        if (memory) for (size_t flop = 0; flop < flops.size(); ++flop) {
            const auto& f = flops[flop];
            const uint8_t held = memory->held[flop];
            if (f.q >= 0) { value[f.q] = held; time[f.q] = memory->held_since[flop]; }
            if (f.q_bar >= 0) { value[f.q_bar] = held > 1 ? UNKNOWN : 1 - held; time[f.q_bar] = memory->held_since[flop]; }
        }
        const auto later = [](Tick at, Tick delay) { return at == NEVER ? NEVER : at + delay; };
        for (const int node : order) {
            const auto& gate = gates[node];
            if (gate.kind == Kind::Hub) {
                value[node] = OPAQUE;
                time[node] = NEVER;
                for (const auto& [in, delay] : leaves[gate.a]) time[node] = std::max(time[node], later(time[in], delay));
                continue;
            }
            const uint8_t a = value[gate.a], b = gate.b >= 0 ? value[gate.b] : a;
            const Tick ta = time[gate.a], tb = gate.b >= 0 ? time[gate.b] : ta;
            // An unknown result is open only if nothing but leaves made it unknown.
            const uint8_t unknown = (a == UNKNOWN || b == UNKNOWN) ? UNKNOWN : OPAQUE;
            // An input at the controlling value decides the output alone, from
            // the moment it is final. Otherwise the output waits for both.
            const auto decided = [&](uint8_t controlling) {
                if (a == controlling && b == controlling) return later(std::min(ta, tb), gate.delay);
                if (a == controlling) return later(ta, gate.delay);
                if (b == controlling) return later(tb, gate.delay);
                return later(std::max(ta, tb), gate.delay);
            };
            switch (gate.kind) {
            case Kind::Buf: value[node] = a; time[node] = later(ta, gate.delay); break;
            case Kind::Not: value[node] = a > 1 ? a : 1 - a; time[node] = later(ta, gate.delay); break;
            case Kind::And: case Kind::Nand: {
                const uint8_t out = (a == 0 || b == 0) ? 0 : (a == 1 && b == 1) ? 1 : unknown;
                value[node] = gate.kind == Kind::And || out > 1 ? out : 1 - out;
                time[node] = decided(0);
                break;
            }
            case Kind::Or: case Kind::Nor: {
                const uint8_t out = (a == 1 || b == 1) ? 1 : (a == 0 && b == 0) ? 0 : unknown;
                value[node] = gate.kind == Kind::Or || out > 1 ? out : 1 - out;
                time[node] = decided(1);
                break;
            }
            default: // Xor
                value[node] = (a > 1 || b > 1) ? unknown : a ^ b;
                time[node] = later(std::max(ta, tb), gate.delay);
            }
        }
        if (memory) {
            // A reset in progress is outside what the flip-flop's timing was measured for.
            for (size_t flop = 0; flop < flops.size(); ++flop) {
                const uint8_t reset = flops[flop].reset >= 0 ? value[flops[flop].reset] : 0;
                if (reset == 0) continue;
                answered = false;
                const uint8_t next = reset == 1 ? 0 : UNKNOWN;
                if (next != memory->held[flop]) { memory->held[flop] = next; memory->held_since[flop] = now; }
            }
            memory->value = value;
            memory->time = time;
            memory->started = true;
        }
        settled.assign(output_nodes.size(), NEVER);
        result.resize(output_nodes.size());
        for (size_t pin = 0; pin < output_nodes.size(); ++pin) {
            result[pin].resize(output_nodes[pin].size());
            for (size_t bit = 0; bit < output_nodes[pin].size(); ++bit) {
                const int node = output_nodes[pin][bit];
                settled[pin] = std::max<long long>(settled[pin], time[node]);
                result[pin][bit] = value[node];
            }
        }
        return answered;
    }
};
} // namespace

std::shared_ptr<const circuit::DelayTwin> buildDelayTwin(const std::shared_ptr<Component>& root, std::string* why) {
    const auto no = [&](const std::string& reason) { if (why) *why = reason; return nullptr; };
    const auto part = std::dynamic_pointer_cast<IOComponent>(root);
    if (!part) return no("not a part with pins");
    GraphRequest request;
    request.check_clocks = false;
    Graph graph;
    std::vector<std::string> diagnostics;
    if (!buildGraph(root, request, graph, diagnostics)) return no(diagnostics.empty() ? "no timing graph" : diagnostics.front());
    if (graph.has_loop) return no("a loop");
    if (!graph.parts.empty()) return no("a memory endpoint inside");
    for (const auto& hub : graph.hubs) if (hub.launch != NEVER) return no("a leaf with a clock that is not a flip-flop");
    if (graph.self_timed || graph.clock_inputs.size() > 1) return no("more than one clock, or a part that times itself");
    if (graph.nodes.size() > LARGEST_TWIN) return no(std::to_string(graph.nodes.size()) + " gate nodes");
    auto twin = std::make_shared<GateTwin>();
    const auto flops_of = [&](size_t index) -> GateTwin::Flop& {
        if (twin->flops.size() <= index) twin->flops.resize(index + 1);
        return twin->flops[index];
    };
    // The flip-flops, found by the pins the graph keeps for them: an output
    // that only launches, a data input that is captured, a reset held low.
    std::unordered_map<const Component*, size_t> flop_of;
    const auto flop = [&](const PinBase* pin) -> GateTwin::Flop* {
        const auto owner = pin ? pin->getOwner() : nullptr;
        if (!owner || typeid(*owner) != typeid(DFlipFlop)) return nullptr;
        return &flops_of(flop_of.emplace(owner.get(), flop_of.size()).first->second);
    };
    twin->gates.reserve(graph.nodes.size());
    std::unordered_set<int> root_inputs;
    for (const auto& [name, pin] : part->getAllInputPins())
        if (const auto first = graph.first_bit.find(pin.get()); first != graph.first_bit.end())
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit) root_inputs.insert(first->second + int(bit));
    for (size_t index = 0; index < graph.nodes.size(); ++index) {
        const auto& node = graph.nodes[index];
        if (node.kind == Kind::Part) return no("a memory endpoint inside");
        const bool source = node.kind == Kind::Stable || node.kind == Kind::Source;
        const bool leaf = node.kind == Kind::Hub;
        if (!source && !leaf && node.a < 0) return no("a gate without inputs");
        twin->gates.push_back({node.kind, leaf ? node.owner : node.a, leaf ? -1 : node.b, source ? 0 : node.delay,
                               uint8_t(node.constant == 0 || node.constant == 1 ? node.constant : UNKNOWN)});
        if (node.kind != Kind::Source || root_inputs.contains(int(index))) continue;
        // A changing bit that is not an input must be a flip-flop's output.
        auto* owner = flop(node.pin);
        if (!owner) {
            // An input pin nothing drives never changes; any other source is
            // a part with state this list cannot follow.
            if (node.pin && node.pin->getType() == PinType::OUTPUT) return no("stored state that is not a plain flip-flop");
            continue;
        }
        const auto name = node.pin->getName();
        (name == "Q" ? owner->q : owner->q_bar) = int(index);
        owner->launch = node.delay;
    }
    if (!graph.groups.empty()) for (const auto& [node, setup] : graph.groups.front().endpoints) {
        auto* owner = flop(graph.nodes[node].pin);
        if (!owner) return no("a captured input that is not a flip-flop's");
        owner->d = node;
    }
    for (const auto& [node, held, name] : graph.required) {
        auto* owner = flop(graph.nodes[node].pin);
        if (!owner || held != 0) return no("a held input that is not a flip-flop's reset");
        owner->reset = node;
    }
    for (const auto& f : twin->flops) if (f.d < 0) return no("a flip-flop without a data input in the graph");
    if (!twin->flops.empty()) {
        if (graph.clock_inputs.size() != 1) return no("flip-flops without one clock input");
        if (graph.nodes.size() > LARGEST_WITH_STATE) return no(std::to_string(graph.nodes.size()) + " gate nodes in a part with state");
    }
    for (const auto& hub : graph.hubs) twin->leaves.push_back(hub.inputs);
    for (const int node : graph.order) {
        const auto kind = graph.nodes[node].kind;
        if (kind != Kind::Stable && kind != Kind::Source) twin->order.push_back(node);
    }
    const auto pins = [&](const std::map<std::string, std::shared_ptr<PinBase>>& all, std::vector<std::pair<std::string, size_t>>& names,
                          std::vector<std::vector<int>>& nodes, bool must_exist) {
        for (const auto& [name, pin] : all) {
            names.emplace_back(name, pin->getWidth());
            auto& bits = nodes.emplace_back(pin->getWidth(), -1);
            const auto first = graph.first_bit.find(pin.get());
            if (first == graph.first_bit.end()) { if (must_exist) return false; continue; }
            for (size_t bit = 0; bit < bits.size(); ++bit) bits[bit] = first->second + int(bit);
        }
        return true;
    };
    pins(part->getAllInputPins(), twin->inputs, twin->input_nodes, false);
    if (!twin->flops.empty()) {
        for (size_t pin = 0; pin < twin->inputs.size(); ++pin) if (twin->inputs[pin].first == graph.clock_inputs.front()) twin->clock_input = int(pin);
        if (twin->clock_input < 0 || twin->inputs[twin->clock_input].second != 1) return no("a clock that is not a one-bit input");
    }
    if (!pins(part->getAllOutputPins(), twin->outputs, twin->output_nodes, true)) return no("an output outside the graph");
    // An input the graph treats as a constant would hide its changes.
    for (const auto& bits : twin->input_nodes) for (const int node : bits)
        if (node >= 0 && graph.nodes[node].kind != Kind::Source && twin->flops.empty()) return no("an input the graph holds constant");
    return twin;
}

} // namespace circuit::analysis
