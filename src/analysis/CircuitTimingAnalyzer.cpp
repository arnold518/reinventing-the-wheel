#include "analysis/CircuitTimingAnalyzer.hpp"

#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <unordered_map>
#include <utility>

namespace circuit::analysis {
namespace {

enum class TimingNodeKind { Combinational, SequentialBoundary };

struct TimingNode {
    std::shared_ptr<Component> component;
    TimingNodeKind kind = TimingNodeKind::Combinational;
    size_t delay = 0;
};

bool isCompactSequential(const std::shared_ptr<Component>& component) {
    if (!std::dynamic_pointer_cast<BasicComponent>(component)) {
        return false;
    }
    const auto& contract = component->getContractId();
    return contract == "memory.write-enabled-bit"
        || contract == "memory.register.width32"
        || contract == "sequential.d-flip-flop"
        || contract == "rv32i.register-file"
        || contract == "rv32i.memory.64k-x32"
        || contract.rfind("rv32i.pipeline-register.", 0) == 0
        || contract == "rv32i.core.educational-five-stage"
        || contract == "rv32i.core.reference";
}

bool isSequentialBoundary(const std::shared_ptr<Component>& component) {
    return component
        && (std::string(component->getTypeName()) == "DFlipFlop"
            || isCompactSequential(component));
}

void collectNodes(
    const std::shared_ptr<Component>& component,
    std::vector<TimingNode>& nodes) {
    if (!component) {
        return;
    }
    if (isSequentialBoundary(component)) {
        nodes.push_back({component, TimingNodeKind::SequentialBoundary, 1});
        return;
    }
    if (const auto basic =
            std::dynamic_pointer_cast<BasicComponent>(component)) {
        nodes.push_back({component, TimingNodeKind::Combinational,
                         basic->getDelay()});
        return;
    }
    for (const auto& child : component->getChildren()) {
        collectNodes(child, nodes);
    }
}

void traceWire(
    size_t source,
    const std::shared_ptr<WireBase>& wire,
    const std::unordered_map<const Component*, size_t>& node_indices,
    std::set<const WireBase*>& visited,
    std::set<std::pair<size_t, size_t>>& edges) {
    if (!wire || !visited.insert(wire.get()).second) {
        return;
    }
    for (const auto& weak_sink : wire->getSinkPinsBase()) {
        const auto sink = weak_sink.lock();
        const auto owner = sink ? sink->getOwner() : nullptr;
        if (!sink || !owner) {
            continue;
        }
        if (const auto found = node_indices.find(owner.get());
            found != node_indices.end()) {
            if (found->second != source) {
                edges.insert({source, found->second});
            }
            continue;
        }
        if (!std::dynamic_pointer_cast<IOComponent>(owner)) {
            continue;
        }
        const auto next = sink->getType() == PinType::INPUT
            ? sink->getInternalWireBase()
            : sink->getExternalWireBase();
        if (next && next.get() != wire.get()) {
            traceWire(source, next, node_indices, visited, edges);
        }
    }
}

std::vector<size_t> reconstruct(
    size_t endpoint,
    const std::vector<size_t>& predecessor,
    size_t node_count) {
    std::vector<size_t> reverse;
    std::set<size_t> seen;
    auto current = endpoint;
    while (current < node_count && seen.insert(current).second) {
        reverse.push_back(current);
        const auto previous = predecessor[current];
        if (previous >= node_count) {
            break;
        }
        current = previous;
    }
    std::reverse(reverse.begin(), reverse.end());
    return reverse;
}

} // namespace

CircuitTimingReport CircuitTimingAnalyzer::analyze(
    const std::shared_ptr<Component>& root) {
    CircuitTimingReport report;
    report.limitation =
        "Normalized component delays only; wire routing, setup/hold, clock "
        "skew, voltage, and physical implementation are outside this model. "
        "Compact stateful memories and register files are one-delay timing "
        "boundaries; select structural fidelity to inspect their internals.";
    if (!root) {
        report.valid = false;
        return report;
    }

    std::vector<TimingNode> nodes;
    collectNodes(root, nodes);
    std::unordered_map<const Component*, size_t> node_indices;
    for (size_t index = 0; index < nodes.size(); ++index) {
        node_indices.emplace(nodes[index].component.get(), index);
        if (nodes[index].kind == TimingNodeKind::SequentialBoundary) {
            ++report.sequential_boundary_count;
        } else {
            ++report.combinational_component_count;
        }
    }

    std::set<std::pair<size_t, size_t>> edges;
    for (size_t source = 0; source < nodes.size(); ++source) {
        const auto io = std::dynamic_pointer_cast<IOComponent>(
            nodes[source].component);
        if (!io) {
            continue;
        }
        std::set<const WireBase*> visited;
        for (const auto& [_, output] : io->getAllOutputPins()) {
            traceWire(source, output->getExternalWireBase(),
                      node_indices, visited, edges);
        }
    }
    report.connection_count = edges.size();

    const auto unreachable = std::numeric_limits<size_t>::max();
    std::vector<size_t> incoming(nodes.size(), 0);
    for (const auto& [source, destination] : edges) {
        (void)source;
        ++incoming[destination];
    }
    std::vector<size_t> arrival(nodes.size(), unreachable);
    std::vector<size_t> predecessor(nodes.size(), nodes.size());
    for (size_t index = 0; index < nodes.size(); ++index) {
        if (nodes[index].kind == TimingNodeKind::SequentialBoundary
            || incoming[index] == 0) {
            arrival[index] = nodes[index].delay;
        }
    }

    size_t critical_endpoint = nodes.size();
    size_t critical_delay = 0;
    const auto consider = [&](size_t endpoint, size_t delay,
                              size_t& best_endpoint, size_t& best_delay) {
        if (delay > best_delay) {
            best_delay = delay;
            best_endpoint = endpoint;
        }
    };
    for (size_t index = 0; index < nodes.size(); ++index) {
        if (arrival[index] != unreachable) {
            consider(index, arrival[index], critical_endpoint,
                     critical_delay);
        }
    }

    bool changed = false;
    for (size_t pass = 0; pass < nodes.size(); ++pass) {
        changed = false;
        for (const auto& [source, destination] : edges) {
            if (arrival[source] == unreachable) {
                continue;
            }
            if (nodes[destination].kind
                == TimingNodeKind::SequentialBoundary) {
                const auto endpoint_delay = arrival[source] + 1;
                if (endpoint_delay > critical_delay) {
                    critical_delay = endpoint_delay;
                    critical_endpoint = destination;
                    predecessor[destination] = source;
                }
                continue;
            }
            const auto candidate = arrival[source]
                + nodes[destination].delay;
            if (arrival[destination] == unreachable
                || candidate > arrival[destination]) {
                arrival[destination] = candidate;
                predecessor[destination] = source;
                changed = true;
                consider(destination, candidate, critical_endpoint,
                         critical_delay);
            }
        }
        if (!changed) {
            break;
        }
        if (pass + 1 == nodes.size()) {
            report.valid = false;
            report.combinational_cycle_detected = true;
        }
    }

    if (report.combinational_cycle_detected) {
        report.critical_path_delay = 0;
        report.critical_path.clear();
        return report;
    }

    report.critical_path_delay = critical_delay;
    if (critical_endpoint < nodes.size()) {
        size_t running_arrival = 0;
        for (const auto index : reconstruct(
                 critical_endpoint, predecessor, nodes.size())) {
            running_arrival += nodes[index].delay;
            report.critical_path.push_back({
                nodes[index].component->getID(),
                nodes[index].component->getTypeName(),
                nodes[index].delay,
                running_arrival,
                nodes[index].kind
                    == TimingNodeKind::SequentialBoundary,
            });
        }
        if (!report.critical_path.empty()) {
            report.critical_path.back().arrival_time = critical_delay;
        }
    }
    return report;
}

} // namespace circuit::analysis
