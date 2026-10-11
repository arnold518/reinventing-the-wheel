#include "analysis/DelayTwin.hpp"
#include "TimingGraph.hpp"
#include "components/IOComponent.hpp"
#include "basic/PinBase.hpp"
#include <algorithm>

namespace circuit::analysis {
namespace {
using namespace detail::timing;

// Values while timing: 0, 1, and 2 for X or Z.
constexpr uint8_t UNKNOWN = 2;
constexpr size_t LARGEST_TWIN = 60000;

class GateTwin final : public circuit::DelayTwin {
public:
    struct Gate { Kind kind; int a, b; Tick delay; uint8_t constant; };
    std::vector<Gate> gates;                 // by node
    std::vector<int> order;                  // gates that are computed, inputs first
    std::vector<std::vector<int>> input_nodes, output_nodes;

    void settle(const std::vector<std::vector<LogicValue>>& values, const std::vector<std::vector<long long>>& changed,
                std::vector<long long>& settled) const override {
        thread_local std::vector<uint8_t> value;
        thread_local std::vector<Tick> time;
        value.resize(gates.size());
        time.resize(gates.size());
        for (size_t node = 0; node < gates.size(); ++node) { value[node] = gates[node].constant; time[node] = NEVER; }
        for (size_t pin = 0; pin < input_nodes.size(); ++pin) for (size_t bit = 0; bit < input_nodes[pin].size(); ++bit) {
            const int node = input_nodes[pin][bit];
            if (node < 0) continue;
            const auto level = values[pin][bit];
            value[node] = level == LogicValue::LOW ? 0 : level == LogicValue::HIGH ? 1 : UNKNOWN;
            time[node] = changed[pin][bit];
        }
        const auto later = [](Tick at, Tick delay) { return at == NEVER ? NEVER : at + delay; };
        for (const int node : order) {
            const auto& gate = gates[node];
            const uint8_t a = value[gate.a], b = gate.b >= 0 ? value[gate.b] : a;
            const Tick ta = time[gate.a], tb = gate.b >= 0 ? time[gate.b] : ta;
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
            case Kind::Not: value[node] = a == UNKNOWN ? UNKNOWN : 1 - a; time[node] = later(ta, gate.delay); break;
            case Kind::And: case Kind::Nand: {
                const uint8_t result = (a == 0 || b == 0) ? 0 : (a == 1 && b == 1) ? 1 : UNKNOWN;
                value[node] = gate.kind == Kind::And || result == UNKNOWN ? result : 1 - result;
                time[node] = decided(0);
                break;
            }
            case Kind::Or: case Kind::Nor: {
                const uint8_t result = (a == 1 || b == 1) ? 1 : (a == 0 && b == 0) ? 0 : UNKNOWN;
                value[node] = gate.kind == Kind::Or || result == UNKNOWN ? result : 1 - result;
                time[node] = decided(1);
                break;
            }
            default: // Xor
                value[node] = (a == UNKNOWN || b == UNKNOWN) ? UNKNOWN : a ^ b;
                time[node] = later(std::max(ta, tb), gate.delay);
            }
        }
        settled.assign(output_nodes.size(), NEVER);
        for (size_t pin = 0; pin < output_nodes.size(); ++pin)
            for (const int node : output_nodes[pin]) settled[pin] = std::max<long long>(settled[pin], time[node]);
    }
};
} // namespace

std::shared_ptr<const circuit::DelayTwin> buildDelayTwin(const std::shared_ptr<Component>& root) {
    const auto part = std::dynamic_pointer_cast<IOComponent>(root);
    if (!part) return nullptr;
    GraphRequest request;
    request.launches = false;
    request.check_clocks = false;
    Graph graph;
    std::vector<std::string> diagnostics;
    if (!buildGraph(root, request, graph, diagnostics)) return nullptr;
    if (graph.has_loop || !graph.hubs.empty() || !graph.parts.empty() || !graph.clock_inputs.empty() || graph.self_timed
        || graph.nodes.size() > LARGEST_TWIN) return nullptr;
    auto twin = std::make_shared<GateTwin>();
    twin->gates.reserve(graph.nodes.size());
    for (const auto& node : graph.nodes) {
        if (node.kind == Kind::Hub || node.kind == Kind::Part) return nullptr;
        const bool source = node.kind == Kind::Stable || node.kind == Kind::Source;
        if (!source && node.a < 0) return nullptr;
        twin->gates.push_back({node.kind, node.a, node.b, source ? 0 : node.delay,
                               uint8_t(node.constant == 0 || node.constant == 1 ? node.constant : UNKNOWN)});
    }
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
    if (!pins(part->getAllOutputPins(), twin->outputs, twin->output_nodes, true)) return nullptr;
    // An input the graph treats as a constant would hide its changes.
    for (const auto& bits : twin->input_nodes) for (const int node : bits)
        if (node >= 0 && graph.nodes[node].kind != Kind::Source) return nullptr;
    return twin;
}

} // namespace circuit::analysis
