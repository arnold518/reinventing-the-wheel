#include "analysis/CircuitTimingAnalyzer.hpp"
#include "TimingEngine.hpp"
#include "basic/PinBase.hpp"
#include "components/IOComponent.hpp"
#include <algorithm>
#include <stdexcept>

namespace circuit::analysis {
namespace {

using namespace detail::timing;

// The settling options, as a description of the circuit's sources.
bool describeSources(const IOComponent& root, const SettlingOptions& options, GraphRequest& request,
                     std::vector<std::string>& diagnostics) {
    const auto input = [&](const std::string& name) { return root.getInputPinDynamic(name).get(); };
    bool valid = true;
    const auto reject = [&](const std::string& message) { diagnostics.push_back(message); valid = false; };
    for (const auto& [name, value] : options.fixed_inputs) {
        if (!input(name)) reject("fixed_inputs names no root input: " + name);
        else request.fixed[input(name)] = value;
    }
    for (const auto& [name, value] : options.final_inputs) {
        if (!input(name) || options.fixed_inputs.contains(name))
            reject("final_inputs must name a root input that is not fixed: " + name);
        else request.final_value[input(name)] = value;
    }
    for (const auto& [name, tick] : options.input_arrival) {
        if (!input(name)) reject("input_arrival names no root input: " + name);
        else if (options.sources != SettlingSources::StateOnly) request.arrival[input(name)] = static_cast<Tick>(tick);
    }
    for (const auto& name : options.held_inputs) {
        if (!input(name) || options.fixed_inputs.contains(name)) reject("held_inputs must name a root input that is not fixed: " + name);
        else request.arrival[input(name)] = NEVER;
    }
    request.contracts = options.contracts;
    request.binary_sources = options.binary_sources;
    request.launches = options.sources != SettlingSources::InputsOnly;
    request.default_arrival = options.sources == SettlingSources::StateOnly ? NEVER : 0;
    return valid;
}

// Two ways of choosing cases; both are sound, so the lower result stands.
// The second runs only if the first leaves the bound open.
GroupResult solve(Engine& engine, const Group& group, const SettlingOptions& options) {
    const bool both = options.search == SettlingSearch::Both;
    auto result = engine.solve(group, options.search == SettlingSearch::Signals,
                               both ? options.search_budget / 2 : options.search_budget);
    if (!both || !result.bounded || !result.changes || result.tight) return result;
    auto other = engine.solve(group, true, options.search_budget - result.evaluations);
    other.evaluations += result.evaluations;
    if (other.bounded && (!other.changes || other.bound < result.bound || (other.bound == result.bound && other.tight)))
        return other;
    result.evaluations = other.evaluations;
    return result;
}

} // namespace

SettlingReport CircuitTimingAnalyzer::settling(const std::shared_ptr<Component>& root,
                                               const SettlingOptions& options) {
    SettlingReport report;
    const auto root_io = std::dynamic_pointer_cast<IOComponent>(root);
    if (!root_io) {
        report.diagnostics.push_back(root ? "Root has no pins" : "No circuit");
        return report;
    }
    try {
        GraphRequest request;
        Graph graph;
        if (!describeSources(*root_io, options, request, report.diagnostics)) return report;
        if (!buildGraph(root, request, graph, report.diagnostics)) return report;
        Engine engine(graph, options.seed);
        engine.evaluateAll();
        for (const auto& [n, value, id] : graph.required) {
            const Fact& fact = engine.fact(n);
            if (fact.settled != NEVER || fact.values != (value ? HIGH : LOW)) {
                report.diagnostics.push_back(id + ": contract requires the fixed value " + std::to_string(value)
                                             + "; state it with fixed_inputs");
                return report;
            }
        }
        const auto first_change = engine.earliest();
        report.clock_inputs = graph.clock_inputs;
        report.clock_outputs = graph.clock_outputs;
        report.self_timed = graph.self_timed;
        report.clocked_interface = graph.self_timed && !graph.self_timed_escapes;
        bool any_endpoint = false;
        for (const auto& group : graph.groups) {
            if (group.endpoints.empty()) continue;
            any_endpoint = true;
            const auto result = solve(engine, group, options);
            if (!result.bounded) {
                report.diagnostics.push_back("Combinational dependency cycle: no finite settling bound for " + group.name);
                report.endpoints.clear();
                return report;
            }
            SettlingEndpoint endpoint;
            endpoint.name = group.name;
            endpoint.changes = result.changes;
            endpoint.structural = static_cast<size_t>(result.structural);
            endpoint.bound = static_cast<size_t>(result.bound);
            endpoint.tight = result.tight;
            Tick first = OPEN;
            for (const auto& [n, extra] : group.endpoints) first = std::min(first, first_change[n]);
            endpoint.earliest = first == OPEN ? 0 : static_cast<size_t>(first);
            endpoint.evaluations = result.evaluations;
            for (const int n : result.path) endpoint.path.push_back(graph.label(n));
            for (size_t var = 0; var < result.witness.size(); ++var)
                if (result.witness[var] < 2 && graph.variables[var].second < 64)
                    endpoint.witness[graph.variables[var].first->getID()]
                        |= uint64_t(result.witness[var]) << graph.variables[var].second;
            report.structural = std::max(report.structural, endpoint.structural);
            report.bound = std::max(report.bound, endpoint.bound);
            report.endpoints.push_back(std::move(endpoint));
        }
        if (!any_endpoint && graph.has_loop) {
            report.diagnostics.push_back("Combinational dependency cycle: no finite settling bound");
            return report;
        }
        report.bit_count = graph.nodes.size();
        report.variable_count = graph.variables.size();
        report.premises = {
            "Every evaluated part publishes f(inputs at t) at exactly t + its delay",
            "The supplied and built-in component contracts are sound",
            "Start-up has passed: logic fed only by unchanging sources is already quiet",
            "One ideal clock taken directly from a root input; rising-edge state only",
            options.binary_sources
                ? "ENVIRONMENT: final root-input and clocked-state values are 0 or 1 (earlier values unrestricted)"
                : "No value restriction: final source values may be 0, 1, X or Z",
            options.sources == SettlingSources::All ? "Root inputs change at their arrival tick and state launches at the clock edge"
                : options.sources == SettlingSources::InputsOnly ? "Only root inputs change; clocked state holds"
                : "Only clocked state launches; root inputs hold"};
        report.valid = true;
    } catch (const std::exception& error) {
        report.diagnostics.push_back(error.what());
        report.valid = false;
        report.endpoints.clear();
    }
    return report;
}

PartTiming CircuitTimingAnalyzer::characterize(const std::shared_ptr<Component>& root,
                                               const SettlingOptions& options) {
    PartTiming timing;
    const auto ask = [&](SettlingSources sources, std::vector<PartDelay>& outputs, size_t& capture) {
        auto asked = options;
        asked.sources = sources;
        const auto report = settling(root, asked);
        if (!report.valid) { timing.diagnostics = report.diagnostics; return false; }
        timing.premises = report.premises;
        for (const auto& endpoint : report.endpoints) {
            if (endpoint.name == "(captures)") { timing.clocked = true; capture = endpoint.bound; continue; }
            outputs.push_back({endpoint.name, endpoint.changes, endpoint.bound, endpoint.tight});
            if (endpoint.changes) timing.delay = std::max(timing.delay, endpoint.bound);
        }
        return true;
    };
    if (!ask(SettlingSources::InputsOnly, timing.input_to_output, timing.input_to_capture)) return timing;
    if (!ask(SettlingSources::StateOnly, timing.clock_to_output, timing.state_to_capture)) return timing;
    for (const auto& output : timing.clock_to_output) timing.clocked = timing.clocked || output.changes;
    timing.premises.pop_back(); // the per-question source premise does not describe both
    timing.valid = true;
    return timing;
}

circuit::PartDelays CircuitTimingAnalyzer::delays(const std::shared_ptr<Component>& root,
                                                  const SettlingOptions& options, const std::string& control,
                                                  std::vector<std::string>* diagnostics) {
    circuit::PartDelays table;
    const auto root_io = std::dynamic_pointer_cast<IOComponent>(root);
    bool failed = false;
    const auto ask = [&](const SettlingOptions& asked) {
        auto report = settling(root, asked);
        if (!report.valid && !failed) {
            failed = true;
            if (diagnostics) *diagnostics = report.diagnostics;
        }
        return report;
    };
    if (!root_io) { ask(options); return {}; }

    // State launches while every input holds.
    auto state = options;
    state.sources = SettlingSources::StateOnly;
    const auto launched = ask(state);
    if (failed) return {};
    for (const auto& endpoint : launched.endpoints) {
        if (endpoint.name == "(captures)") table.internal_period = endpoint.bound;
        else if (endpoint.changes) table.launches[endpoint.name] = {endpoint.earliest, endpoint.bound};
    }
    if (!launched.clock_inputs.empty()) table.clock = launched.clock_inputs.front();
    table.clock_outputs = launched.clock_outputs;
    table.self_timed = launched.self_timed;
    table.clocked_interface = launched.clocked_interface;

    // One input changes while the others, and the state, hold.
    const auto pairs = [&](const SettlingOptions& base, const std::string& only) {
        std::vector<circuit::PartDelays::Arc> arcs;
        for (const auto& [changing, pin] : root_io->getAllInputPins()) {
            if (changing == table.clock || base.fixed_inputs.contains(changing)) continue;
            if (!only.empty() && changing != only) continue;
            auto one = base;
            one.sources = SettlingSources::InputsOnly;
            one.held_inputs.clear();
            for (const auto& [name, other] : root_io->getAllInputPins())
                if (name != changing && name != table.clock && !base.fixed_inputs.contains(name)) one.held_inputs.push_back(name);
            const auto report = ask(one);
            if (failed) return arcs;
            for (const auto& endpoint : report.endpoints) {
                if (!endpoint.changes) continue;
                if (endpoint.name == "(captures)") {
                    auto& setup = table.setup[changing];
                    setup = std::max(setup, endpoint.bound);
                } else arcs.push_back({changing, endpoint.name, {endpoint.earliest, endpoint.bound}});
            }
        }
        return arcs;
    };
    table.arcs = pairs(options, "");
    if (!control.empty() && !failed) {
        const auto pin = root_io->getInputPinDynamic(control);
        if (!pin || pin->getWidth() > 8) {
            if (diagnostics) diagnostics->push_back("control must name an input of at most 8 bits: " + control);
            return {};
        }
        for (uint64_t value = 0; value < (uint64_t{1} << pin->getWidth()) && !failed; ++value) {
            // The control input itself changing and ending at this value...
            auto ending = options;
            ending.final_inputs[control] = value;
            auto arcs = pairs(ending, control);
            // ...and every other input changing while it holds this value.
            auto holding = options;
            holding.fixed_inputs[control] = value;
            for (auto& arc : pairs(holding, "")) arcs.push_back(std::move(arc));
            table.arcs_by_control.emplace(value, std::move(arcs));
        }
        table.control = control;
    }
    if (failed) return {};
    table.fixed_inputs = options.fixed_inputs;
    table.evidence = "proven on " + root->getImplementationId();
    return table;
}

} // namespace circuit::analysis
