#include "analysis/CircuitTimingAnalyzer.hpp"
#include "TimingCircuit.hpp"
#include "TimingEngine.hpp"
#include "MemoryTimingState.hpp"
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "../modules/memory/FlipFlopCellBehavioral.hpp"
#include "modules/storage/RegisterBehavioral.hpp"
#include "modules/storage/ClockedStateBitBehavioral.hpp"
#include "../modules/rv32i/RegisterFile32x32Behavioral.hpp"
#include <algorithm>
#include <array>
#include <deque>
#include <limits>
#include <queue>
#include <set>
#include <stdexcept>
#include <typeinfo>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace circuit::analysis {
namespace {
constexpr size_t none = std::numeric_limits<size_t>::max();
size_t add(size_t a, size_t b) {
    if (b > none - a) throw std::overflow_error("Timing arithmetic overflow");
    return a + b;
}
size_t twice(size_t n) { return add(n, n); }
size_t phase(ClockEdge edge) {
    if (edge != ClockEdge::Rising && edge != ClockEdge::Falling)
        throw std::invalid_argument("Invalid clock edge");
    return edge == ClockEdge::Rising ? 0 : 1;
}
void validate(DelayBounds d) {
    if (d.minimum > d.maximum) throw std::invalid_argument("Reversed delay bounds");
}

struct Arrival {
    bool valid = false;
    size_t early = 0, late = 0, previous = none;
    size_t early_previous = none;
};
struct Node {
    std::shared_ptr<PinBase> pin;
    std::array<Arrival, 2> arrival;
    bool stable = false;
    std::optional<uint64_t> value;
};
struct Edge { size_t to; DelayBounds delay; bool wire; };
using Leaf = detail::TimingLeaf;

// The reachable states of the memory handshakes: together they cover every
// situation, and the period must satisfy each of them.
struct StateTiming {
    std::vector<detail::MemoryTimingGroup> groups;
    size_t case_count = 1;
};

class Analysis {
    const std::shared_ptr<Component>& root;
    const ClockSpecification& clock;
    const TimingContracts& models;
    ClockTimingReport report;
    std::vector<Leaf> leaves;
    std::vector<Node> nodes;
    std::vector<std::vector<Edge>> edges;
    std::unordered_map<std::shared_ptr<PinBase>, size_t> indices;
    std::map<std::string, std::shared_ptr<PinBase>> pins;
    std::unordered_set<const PinBase*> leaf_outputs;
    std::vector<std::pair<size_t, ClockCapture>> captures;
    std::vector<std::pair<size_t, uint64_t>> fixed;
    std::set<size_t> clock_nodes, unused;
    size_t period = 2, clock_node = none;
    bool unknown = false, violation = false;
    bool state_timing_checked = false;
    // Per launch edge: earliest transitions, and the shared engine.
    std::vector<Node> initial;
    std::array<std::vector<size_t>, 2> early, early_previous;
    std::array<std::unique_ptr<detail::timing::Graph>, 2> graph;
    std::array<std::unique_ptr<detail::timing::Engine>, 2> engine;
    std::vector<bool> changes;
    size_t case_splits = 0;

    void diagnostic(const std::string& message) {
        unknown = true;
        if (report.diagnostics.size() < 64) report.diagnostics.push_back(message);
    }
    size_t node(const std::shared_ptr<PinBase>& pin) {
        if (auto it = indices.find(pin); it != indices.end()) return it->second;
        if (nodes.size() >= clock.maximum_nodes) throw std::runtime_error("Timing node budget exhausted");
        const size_t n = nodes.size(); indices.emplace(pin, n);
        nodes.push_back({pin, {}, false, {}}); edges.emplace_back();
        return n;
    }
    void connect(size_t a, size_t b, DelayBounds d, bool wire = false) {
        validate(d);
        if (report.edge_count >= clock.maximum_edges) throw std::runtime_error("Timing edge budget exhausted");
        edges[a].push_back({b, d, wire}); ++report.edge_count;
    }
    void collect(const std::shared_ptr<Component>& component) {
        detail::collectTimingLeaves(component, models, clock.maximum_hierarchy_depth, leaves, {
            [&](const std::shared_ptr<IOComponent>& io) {
                for (const auto* group : {&io->getAllInputPins(), &io->getAllOutputPins()})
                    for (const auto& [name, p] : *group) {
                        if (pins.size() >= clock.maximum_nodes) throw std::runtime_error("Timing pin budget exhausted");
                        if (!pins.emplace(p->getID(), p).second) throw std::runtime_error("Duplicate pin ID: " + p->getID());
                    }
            },
            [&](const Component& part) {
                diagnostic(part.getID() + ": no all-state timing contract for " + part.getTypeName());
            }});
    }
    std::shared_ptr<PinBase> lookup(const std::string& id, PinType type) {
        auto it = pins.find(id);
        // Accept the existing timing-report shorthand COMPONENT.PIN as well
        // as canonical COMPONENT.input.PIN / COMPONENT.output.PIN IDs.
        if (it == pins.end()) {
            const auto dot = id.rfind('.');
            if (dot != std::string::npos) it = pins.find(id.substr(0, dot)
                + (type == PinType::INPUT ? ".input" : ".output") + id.substr(dot));
        }
        if (it == pins.end() || it->second->getType() != type) throw std::runtime_error("Invalid timing pin: " + id);
        return it->second;
    }
    // The modeled leaf output or undriven input that feeds `p`.
    std::shared_ptr<PinBase> source(const std::shared_ptr<PinBase>& p) {
        auto driver = detail::drivingPin(p, leaf_outputs);
        if (driver->getType() == PinType::OUTPUT && !leaf_outputs.contains(driver.get()))
            throw std::runtime_error("Undriven output: " + driver->getID());
        return driver;
    }
    void bindContracts() {
        for (const auto& leaf : leaves) {
            const auto& c = leaf.component;
            for (const auto& [n, p] : c->getAllInputPins()) node(p);
            for (const auto& [n, p] : c->getAllOutputPins()) { node(p); leaf_outputs.insert(p.get()); }
        }
        for (const auto& leaf : leaves) {
            const auto& c = leaf.component; const auto& m = leaf.contract;
            if (m.evidence.empty()) { diagnostic(c->getID() + ": contract needs evidence/version"); continue; }
            if (m.self_timed) {
                diagnostic(c->getID() + ": self-timed part; its outputs change at times unrelated to the clock");
                continue;
            }
            report.contracts.push_back(c->getImplementationId() + ": " + m.evidence);
            std::set<std::string> inputs, outputs;
            auto in = [&](const std::string& n) { inputs.insert(n); return node(lookup(c->getID() + "." + n, PinType::INPUT)); };
            auto out = [&](const std::string& n) { outputs.insert(n); return node(lookup(c->getID() + "." + n, PinType::OUTPUT)); };
            if (!m.clock_pin.empty()) {
                const auto n = in(m.clock_pin);
                if (nodes[n].pin->getWidth() != 1) diagnostic(c->getID() + ": clock pin must have width one");
                clock_nodes.insert(n);
                if (std::find(m.unused_inputs.begin(), m.unused_inputs.end(), m.clock_pin) != m.unused_inputs.end())
                    diagnostic(c->getID() + ": clock pin cannot be unused");
            }
            if ((!m.captures.empty() || !m.launches.empty()) && m.clock_pin.empty())
                diagnostic(c->getID() + ": state timing lacks a clock pin");
            for (const auto& arc : m.arcs) connect(in(arc.input), out(arc.output), arc.delay);
            for (const auto& cap : m.captures) { phase(cap.edge); captures.emplace_back(in(cap.pin), cap); }
            for (const auto& launch : m.launches) {
                validate(launch.delay);
                auto& a = nodes[out(launch.pin)].arrival[phase(launch.edge)];
                if (a.valid) throw std::runtime_error("Duplicate launch contract: " + c->getID());
                a = {true, launch.delay.minimum, launch.delay.maximum, none};
            }
            for (const auto& [n, v] : m.fixed_inputs) {
                const auto i = in(n);
                if (nodes[i].pin->getWidth() < 64 && v >= (uint64_t{1} << nodes[i].pin->getWidth()))
                    throw std::runtime_error("Fixed contract value exceeds pin width: " + nodes[i].pin->getID());
                fixed.emplace_back(i, v);
            }
            for (const auto& n : m.unused_inputs) unused.insert(in(n));
            for (const auto& n : m.stable_outputs) nodes[out(n)].stable = true;
            // A passed-on clock is not data. A part clocked from it is still
            // refused below: this analysis takes the clock from the root only.
            for (const auto& n : m.clock_outputs) nodes[out(n)].stable = true;
            for (const auto& [n, v] : m.constant_outputs) {
                const auto i = out(n);
                if (nodes[i].pin->getWidth() < 64 && v >= (uint64_t{1} << nodes[i].pin->getWidth()))
                    throw std::runtime_error("Constant contract value exceeds pin width: " + nodes[i].pin->getID());
                nodes[i].stable = true; nodes[i].value = v;
            }
            for (const auto& [n, p] : c->getAllInputPins()) if (!inputs.contains(n))
                diagnostic(p->getID() + ": input absent from timing contract");
            for (const auto& [n, p] : c->getAllOutputPins()) if (!outputs.contains(n))
                diagnostic(p->getID() + ": output absent from timing contract");
            for (const auto& arc : m.arcs) {
                if (arc.input == m.clock_pin || std::find(m.unused_inputs.begin(), m.unused_inputs.end(), arc.input) != m.unused_inputs.end())
                    diagnostic(c->getID() + ": clock/unused pin cannot also be a data arc");
            }
            for (const auto& cap : m.captures) {
                if (cap.pin == m.clock_pin || std::find(m.unused_inputs.begin(), m.unused_inputs.end(), cap.pin) != m.unused_inputs.end())
                    diagnostic(c->getID() + ": clock/unused pin cannot also be captured");
            }
            const size_t pulse = twice(std::max(m.minimum_high_ticks, m.minimum_low_ticks));
            if (pulse > period) { period = pulse; report.limiting_constraint = c->getID() + ": clock pulse width"; }
        }
    }
    void bindBoundary() {
        const auto cp = lookup(clock.clock_pin, PinType::INPUT);
        if (cp->getWidth() != 1) throw std::runtime_error("Clock input must have width one");
        if (source(cp) != cp) throw std::runtime_error("Clock must be an external input: " + clock.clock_pin);
        clock_node = node(cp);
        std::set<size_t> declared;
        for (const auto& input : clock.inputs) {
            auto p = lookup(input.pin, PinType::INPUT);
            if (source(p) != p) throw std::runtime_error("Cannot constrain an internally driven input: " + input.pin);
            auto n = node(p);
            if (n == clock_node || !declared.insert(n).second) throw std::runtime_error("Duplicate/clock input constraint: " + input.pin);
            validate(input.delay);
            if (!input.edge && p->getWidth() < 64 && input.fixed_value >= (uint64_t{1} << p->getWidth()))
                throw std::runtime_error("Fixed input value exceeds pin width: " + input.pin);
            if (input.edge) {
                nodes[n].arrival[phase(*input.edge)] = {true, input.delay.minimum, input.delay.maximum, none};
                const auto input_period = add(input.delay.maximum, clock.event_separation_ticks);
                if (input_period > period) {
                    period = input_period;
                    report.limiting_constraint = input.pin + ": external transition window";
                }
                report.assumptions.push_back(input.pin + ": transitions " + std::to_string(input.delay.minimum)
                    + ".." + std::to_string(input.delay.maximum) + " ticks after "
                    + (*input.edge == ClockEdge::Rising ? "rising" : "falling") + " edge");
            } else {
                nodes[n].stable = true; nodes[n].value = input.fixed_value;
                report.assumptions.push_back(input.pin + " fixed at " + std::to_string(input.fixed_value));
            }
        }
        for (const auto& leaf : leaves) for (const auto& [name, p] : leaf.component->getAllInputPins()) {
            const auto n = node(p);
            if (unused.contains(n)) continue;
            const auto src = source(p); const auto s = node(src);
            if (clock_nodes.contains(n)) {
                if (s != clock_node) diagnostic(p->getID() + ": derived, gated, or different clock is unsupported");
            } else if (s == clock_node) diagnostic(p->getID() + ": clock used as data requires an explicit clock-as-data model");
            else if (s != n) connect(s, n, {0, 0}, true);
            else if (!declared.contains(s)) diagnostic(p->getID() + ": unconstrained external/undriven input");
        }
        std::set<std::string> external_outputs;
        for (const auto& cap : clock.outputs) {
            phase(cap.edge);
            const auto p = lookup(cap.pin, PinType::OUTPUT);
            if (p->getOwner() != root || !external_outputs.insert(p->getID()).second)
                throw std::runtime_error("Output constraint must name a unique root output: " + cap.pin);
            const auto n = node(p), s = node(source(p));
            if (s != n) connect(s, n, {0, 0}, true);
            captures.emplace_back(n, cap);
            report.assumptions.push_back(cap.pin + ": sampled on "
                + (cap.edge == ClockEdge::Rising ? "rising" : "falling")
                + " edge, setup " + std::to_string(cap.setup) + ", hold " + std::to_string(cap.hold));
        }
        if (const auto io = std::dynamic_pointer_cast<IOComponent>(root))
            for (const auto& [name, p] : io->getAllOutputPins()) if (!external_outputs.contains(p->getID()))
                diagnostic(p->getID() + ": external output has no sampling requirement");
    }
    void validateBinaryStartup() {
        if(!clock.binary_startup)return;
        report.assumptions.push_back("Binary-startup scope: assert reset on all modeled storage, release it, then allow initial settling before the first operating edge; no X/Z input injection or memory mutation");
        for(const auto& input:clock.inputs)if(input.edge) {
            if(!input.binary_values)diagnostic(input.pin+": binary startup requires an explicit binary_values environment contract");
            else report.assumptions.push_back(input.pin+": external transitions restricted to binary values");
        }
        for(const auto& leaf:leaves) {
            const auto& c=leaf.component;
            if(!leaf.builtin_rules) {diagnostic(c->getID()+": binary startup does not support replacement component contracts");continue;}
            if(auto ep=std::dynamic_pointer_cast<BehavioralMemoryEndpoint>(c)) {
                if(ep->hasStarted())diagnostic(c->getID()+": binary startup requires a pre-simulation provisioned memory image");
                else if(!ep->memoryStorage().isFullyBinary())diagnostic(c->getID()+": initial memory image contains X/Z; binary startup rejected");
                else report.binary_evidence.push_back(c->getID()+": checked all "+std::to_string(ep->memoryStorage().capacityBytes())+" bytes known binary, including sparse default fill");
            } else if(!leaf.contract.launches.empty()) {
                if(typeid(*c)!=typeid(FlipFlopCellBehavioral)&&typeid(*c)!=typeid(RegisterBehavioral)&&typeid(*c)!=typeid(RegisterFile32x32Behavioral)
                    &&typeid(*c)!=typeid(ClockedStateBitBehavioral))
                    diagnostic(c->getID()+": no reset/binary preservation model");
            }
        }
        report.binary_evidence.push_back("Exact FF/register/register-file reset seeds are zero; endpoint FULL/saved response reset seeds are zero. Binary next-state data and enables must be proved at every capture before certification.");
    }
    // ---- Memory-mode timing ------------------------------------------------
    // A memory endpoint is either empty or holding a response, and its timing
    // arcs differ between the two. Each combination of modes is settled and
    // checked on its own; the period must satisfy all of them.

    // nullopt: the mode analysis applies. Otherwise the value to return:
    // true = finished here (with a diagnostic), false = use the general analysis.
    std::optional<bool> collectMemoryGroups(StateTiming& work) {
        // State changes in these models occur on rising edges. Do not silently
        // apply this solver to a mixed-edge environment; the general DAG
        // analyzer below remains available for such circuits.
        for (const auto& n : nodes) if (n.arrival[1].valid) {
            if (clock.binary_startup) { diagnostic("Binary startup currently requires rising-edge launches"); return true; }
            return false;
        }
        // Explicit replacement contracts are authoritative. Do not combine
        // their delays with unrequested native semantic refinements.
        for (const auto& leaf : leaves) if (!leaf.builtin_rules) return false;
        for (const auto& leaf : leaves) {
            const auto ep = std::dynamic_pointer_cast<BehavioralMemoryEndpoint>(leaf.component);
            if (!ep) continue;
            auto group = detail::memoryTimingStates(ep, clock);
            if (!group) {
                if (clock.binary_startup) {
                    diagnostic(ep->getID() + ": no supported binary memory-control state model");
                    return true;
                }
                return false;
            }
            if (group->cases.size() > clock.maximum_state_cases / work.case_count) {
                diagnostic("Memory timing state-case budget exhausted");
                return true;
            }
            work.case_count *= group->cases.size();
            work.groups.push_back(std::move(*group));
        }
        if (work.groups.empty() && !clock.binary_startup) return false;
        return std::nullopt;
    }

    void recordStatePremises(const StateTiming& work) {
        for (const auto& g : work.groups) report.state_evidence.push_back(g.evidence);
        // Binary next-state bits alone do not discharge the premises used to
        // enumerate binary controller inputs. Check those inputs at the edge
        // too, including their settling, binary-domain arrival and hold bounds.
        std::set<size_t> checked_controls;
        for (const auto& g : work.groups) for (const auto& p : g.binary_controls) {
            const auto n = indices.at(source(p));
            if (checked_controls.insert(n).second)
                captures.emplace_back(n, ClockCapture{p->getID(), ClockEdge::Rising, 0, 0});
        }
        if (!checked_controls.empty()) report.binary_evidence.push_back(
            std::to_string(checked_controls.size())
            + " quantified control inputs checked binary and timed at each edge before using binary-only reachability");
        report.state_evidence.push_back(clock.binary_startup
            ? "Timed per-bit routing and FF validator cofactors under checked binary-startup premises"
            : "Timed per-bit routing and four-state FF validator cofactors; no binary-input assumption");
        report.state_case_count = work.case_count;
    }

    // Earliest transitions retain EVERY all-state arc, including feedback and
    // the arcs of every memory mode. A shortest-path search works with loops
    // and must not discard a hold-time hazard.
    void earliestArrivals() {
        initial = nodes;
        for (size_t k = 0; k < 2; ++k) {
            early[k].assign(nodes.size(), none);
            early_previous[k].assign(nodes.size(), none);
            using Entry = std::pair<size_t, size_t>;
            std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> heap;
            for (size_t n = 0; n < nodes.size(); ++n) if (nodes[n].arrival[k].valid) {
                early[k][n] = nodes[n].arrival[k].early;
                heap.emplace(early[k][n], n);
            }
            while (!heap.empty()) {
                const auto [time, n] = heap.top();
                heap.pop();
                if (time != early[k][n]) continue;
                for (const auto& e : edges[n]) {
                    const auto t = add(time, e.delay.minimum);
                    if (t < early[k][e.to]) {
                        early[k][e.to] = t;
                        early_previous[k][e.to] = n;
                        heap.emplace(t, e.to);
                    }
                }
            }
        }
    }

    // The circuit as the shared engine sees it, for launches on one edge.
    bool buildEngine(size_t k) {
        using namespace detail::timing;
        GraphRequest request;
        request.contracts = models;
        request.binary_sources = clock.binary_startup;
        request.phase = k == 0 ? ClockEdge::Rising : ClockEdge::Falling;
        request.other_edge_is_error = false;
        request.check_clocks = false;      // checked above, with this analysis' own messages
        request.default_arrival = NEVER;   // every changing input is declared
        for (const auto& input : clock.inputs) {
            const auto pin = lookup(input.pin, PinType::INPUT).get();
            if (!input.edge) request.fixed[pin] = input.fixed_value;
            else request.arrival[pin] = phase(*input.edge) == k ? static_cast<Tick>(input.delay.maximum) : NEVER;
        }
        graph[k] = std::make_unique<Graph>();
        std::vector<std::string> problems;
        if (!buildGraph(root, request, *graph[k], problems)) {
            for (const auto& problem : problems) diagnostic(problem);
            return false;
        }
        engine[k] = std::make_unique<Engine>(*graph[k]);
        return true;
    }

    // Latest arrivals of one covering case, from the shared engine. False:
    // no result (a diagnostic says why).
    bool settleCase(size_t k, const detail::timing::Case* known) {
        using namespace detail::timing;
        const auto& g = *graph[k];
        // Every pin must settle, not only the captured ones: an oscillator
        // anywhere means the circuit has no quiet state to certify.
        std::vector<int> needed;
        for (size_t n = 0; n < nodes.size(); ++n) {
            if (n == clock_node || clock_nodes.contains(n) || unused.contains(n)) continue;
            if (const auto first = g.first_bit.find(nodes[n].pin.get()); first != g.first_bit.end())
                for (size_t bit = 0; bit < nodes[n].pin->getWidth(); ++bit) needed.push_back(first->second + int(bit));
        }
        engine[k]->setCase(known);
        std::vector<Fact> facts;
        size_t splits = 0;
        const size_t budget = clock.maximum_state_evaluations;
        const auto outcome = engine[k]->resolve(needed, budget, facts, splits);
        report.state_evaluations = engine[k]->work();
        case_splits += splits;
        if (outcome == Engine::Resolved::OutOfWork) { diagnostic("State settling work budget exhausted"); return false; }
        if (outcome == Engine::Resolved::No) {
            if (clock.binary_startup) diagnostic("Binary-startup settling/preservation proof incomplete");
            else {
                diagnostic("Combinational dependency cycle: no finite all-state settling bound established");
                reportDependencyCycle(k);
            }
            return false;
        }
        for (size_t n = 0; n < nodes.size(); ++n) {
            const auto first = g.first_bit.find(nodes[n].pin.get());
            if (n == clock_node || clock_nodes.contains(n) || first == g.first_bit.end()) continue;
            Tick late = NEVER;
            bool open = false, decided = true;
            uint64_t value = 0;
            for (size_t bit = 0; bit < nodes[n].pin->getWidth(); ++bit) {
                const Fact& f = facts[first->second + bit];
                open |= f.settled == OPEN;
                late = std::max(late, f.settled);
                if (f.values == HIGH && bit < 64) value |= uint64_t{1} << bit;
                else if (f.values != LOW) decided = false;
            }
            auto& arrival = nodes[n].arrival[k];
            const size_t earliest = early[k][n];
            // Proven never to change here, yet reachable in some other mode:
            // keep the earliest transition so that hold is still checked.
            if (late == NEVER && earliest == none) arrival.valid = false;
            else arrival = {true, earliest == none ? 0 : earliest,
                            open ? none : late == NEVER ? earliest : static_cast<size_t>(late), none, early_previous[k][n]};
            if (late != NEVER) changes[n] = true;
            else if (decided && !changes[n]) nodes[n].value = value;
        }
        if (clock.binary_startup) for (const auto& [n, capture] : captures) {
            const auto first = g.first_bit.find(nodes[n].pin.get());
            if (first == g.first_bit.end()) continue;
            for (size_t bit = 0; bit < nodes[n].pin->getWidth(); ++bit) {
                const Fact& f = facts[first->second + bit];
                if (f.values & (X | Z)) {
                    diagnostic(nodes[n].pin->getID() + "[" + std::to_string(bit) + "]: binary next-state preservation unproved");
                    return false;
                }
                // Domain restrictions must arrive before capture too, not just
                // the possibly earlier value-settling deadline.
                auto& arrival = nodes[n].arrival[k];
                if (arrival.valid && f.at != NEVER) arrival.late = std::max(arrival.late, static_cast<size_t>(f.at));
                ++report.binary_capture_bits;
            }
        }
        return true;
    }
    void markStable() {
        for (size_t n = 0; n < nodes.size(); ++n) {
            if (n == clock_node || clock_nodes.contains(n)) continue;
            nodes[n].stable = !changes[n];
            if (changes[n]) nodes[n].value.reset();
        }
    }

    // A real closed walk among the pins that stayed open.
    void reportDependencyCycle(size_t k) {
        using namespace detail::timing;
        const auto& g = *graph[k];
        std::vector<bool> open(nodes.size());
        for (size_t n = 0; n < nodes.size(); ++n)
            if (const auto first = g.first_bit.find(nodes[n].pin.get()); first != g.first_bit.end())
                for (size_t bit = 0; bit < nodes[n].pin->getWidth(); ++bit)
                    open[n] = open[n] || engine[k]->fact(first->second + int(bit)).settled == OPEN;
        std::vector<unsigned char> color(nodes.size());
        std::vector<std::pair<size_t, size_t>> stack;
        for (size_t start = 0; start < nodes.size() && report.dependency_cycle.empty(); ++start) {
            if (!open[start] || color[start]) continue;
            stack.emplace_back(start, 0);
            color[start] = 1;
            while (!stack.empty() && report.dependency_cycle.empty()) {
                auto& [n, next] = stack.back();
                if (next == edges[n].size()) { color[n] = 2; stack.pop_back(); continue; }
                const auto target = edges[n][next++].to;
                if (!open[target] || color[target] == 2) continue;
                if (!color[target]) { color[target] = 1; stack.emplace_back(target, 0); continue; }
                auto begin = std::find_if(stack.begin(), stack.end(), [&](const auto& frame) { return frame.first == target; });
                for (auto it = begin; it != stack.end(); ++it) report.dependency_cycle.push_back(nodes[it->first].pin->getID());
                report.dependency_cycle.push_back(nodes[target].pin->getID());
            }
        }
    }

    // One reachable state of every memory handshake, as the engine's case.
    // False: the circuit does not fit the mode model.
    bool memoryCase(const StateTiming& work, size_t mode, detail::timing::Case& known) {
        using namespace detail::timing;
        const auto& g = *graph[0];
        known.binary_parts = clock.binary_startup;
        known.part_mode.assign(g.parts.size(), AnyMode);
        size_t selection = mode;
        for (const auto& group : work.groups) {
            const auto& c = group.cases[selection % group.cases.size()];
            selection /= group.cases.size();
            for (const auto& [pin, value] : c.registers) {
                const auto first = g.first_bit.find(pin.get());
                if (first == g.first_bit.end() || g.nodes[first->second].kind != Kind::Source) return false;
                known.known.emplace_back(first->second, static_cast<uint8_t>(1u << unsigned(value)));
            }
            for (size_t part = 0; part < g.parts.size(); ++part)
                if (g.parts[part].endpoint == group.endpoint.get()) known.part_mode[part] = c.full ? Full : Empty;
        }
        return true;
    }

    void propagate() {
        std::vector<size_t> indegree(nodes.size());
        for (const auto& es : edges) for (const auto& e : es) ++indegree[e.to];
        for (size_t i = 0; i < nodes.size(); ++i) if (!indegree[i] && i != clock_node
            && !clock_nodes.contains(i) && !unused.contains(i) && !nodes[i].stable
            && !nodes[i].arrival[0].valid && !nodes[i].arrival[1].valid)
            diagnostic(nodes[i].pin->getID() + ": unconstrained source has no transition bounds");
        for (size_t i = 0; i < nodes.size(); ++i) if (nodes[i].stable &&
            (indegree[i] || nodes[i].arrival[0].valid || nodes[i].arrival[1].valid))
            diagnostic(nodes[i].pin->getID() + ": contradictory stable and changing drivers");
        if (unknown) return;

        StateTiming work;
        const auto decided = collectMemoryGroups(work);
        if (decided && *decided) return;
        const bool modes = !decided;
        if (modes) recordStatePremises(work);
        earliestArrivals();
        bool falling = false;
        for (const auto& n : nodes) falling |= n.arrival[1].valid;
        if (!buildEngine(0) || (falling && !buildEngine(1))) return;

        for (size_t mode = 0; mode < (modes ? work.case_count : 1); ++mode) {
            nodes = initial;
            changes.assign(nodes.size(), false);
            detail::timing::Case known;
            const bool fits = modes && memoryCase(work, mode, known);
            if (!settleCase(0, fits ? &known : nullptr) || (falling && !settleCase(1, nullptr))) {
                if (modes) report.state_evidence.push_back("State case " + std::to_string(mode)
                    + ": no bounded settling proof");
                nodes = initial;
                return;
            }
            markStable();
            if (!modes) return; // run() checks the single general case
            ++report.checked_state_cases;
            checkTiming();
            if (unknown || violation) { state_timing_checked = true; return; }
        }
        state_timing_checked = true;
        if (case_splits) report.state_evidence.push_back(std::to_string(case_splits)
            + " timed controlling-value implications proved from native gate wiring");
        report.assumptions.push_back(clock.binary_startup
            ? "All binary memory control cases and captured-bit preservation checked with timed settling; hold uses all uncut arcs"
            : "All reachable four-state memory control cases checked with timed settling; hold uses all uncut arcs");
    }
    void path(size_t n, size_t k, bool earliest = false) {
        report.critical_path.clear();
        if (earliest) {
            for (; n != none; n = early_previous[k][n]) report.critical_path.push_back(nodes[n].pin->getID());
            std::reverse(report.critical_path.begin(), report.critical_path.end());
            return;
        }
        // The latest bit of the pin, traced back through the engine.
        using namespace detail::timing;
        const auto& g = *graph[k];
        const auto first = g.first_bit.find(nodes[n].pin.get());
        if (first == g.first_bit.end()) return;
        int latest = first->second;
        for (size_t bit = 1; bit < nodes[n].pin->getWidth(); ++bit)
            if (engine[k]->fact(first->second + int(bit)).settled > engine[k]->fact(latest).settled) latest = first->second + int(bit);
        for (const int step : engine[k]->pathTo(latest)) {
            auto id = g.nodes[step].pin->getID();
            if (report.critical_path.empty() || report.critical_path.back() != id) report.critical_path.push_back(std::move(id));
        }
    }
    void checkTiming() {
        std::optional<std::pair<size_t, size_t>> failed_hold;
        for (const auto& [n, v] : fixed) if (!nodes[n].stable || !nodes[n].value || *nodes[n].value != v)
            diagnostic(nodes[n].pin->getID() + ": contract requires fixed value " + std::to_string(v));
        for (const auto& [n, cap] : captures) {
            ++report.capture_count;
            if (!nodes[n].stable && !nodes[n].arrival[0].valid && !nodes[n].arrival[1].valid)
                diagnostic(nodes[n].pin->getID() + ": capture has no constrained source");
            for (size_t k = 0; k < 2; ++k) {
                const auto& a = nodes[n].arrival[k]; if (!a.valid) continue;
                ++report.checked_setup_relationships; ++report.checked_hold_relationships;
                const bool same = k == phase(cap.edge);
                auto required = add(add(a.late, cap.setup), clock.event_separation_ticks);
                if (!same) required = twice(required);
                if (required > period) {
                    period = required; path(n, k);
                    report.limiting_constraint = nodes[n].pin->getID() + (same ? ": full-cycle setup" : ": half-cycle setup");
                }
                const auto hold = add(cap.hold, clock.event_separation_ticks);
                if (a.early < hold) {
                    if (same) {
                        violation = true;
                        if (!failed_hold) failed_hold = {n, k};
                        if (report.diagnostics.size() < 64) report.diagnostics.push_back(nodes[n].pin->getID()
                            + ": same-edge hold bound fails (earliest " + std::to_string(a.early)
                            + ", required " + std::to_string(hold) + "); increasing the period cannot fix it");
                    } else {
                        const auto required_hold = twice(hold - a.early);
                        if (required_hold > period) {
                            period = required_hold; report.critical_path.clear();
                            report.limiting_constraint = nodes[n].pin->getID() + ": opposite-edge hold";
                        }
                    }
                }
            }
        }
        if (failed_hold) {
            path(failed_hold->first, failed_hold->second, true);
            report.limiting_constraint = nodes[failed_hold->first].pin->getID() + ": same-edge hold";
        }
        if (!report.capture_count) diagnostic("No clocked captures or external sampling requirements");
    }
public:
    Analysis(const std::shared_ptr<Component>& r, const ClockSpecification& c, const TimingContracts& m)
        : root(r), clock(c), models(m) {}
    ClockTimingReport run() {
        report.assumptions = {
            "Fixed circuit and sound all-state component contracts; no claim of physical Fmax or functional correctness",
            "One ideal directly distributed clock, 50% duty cycle; no skew or jitter",
            "Clock source: " + clock.clock_pin,
            "Reset/initialization completed and all combinational logic settled before operation",
            "Only declared synchronous input transitions; no memory/debug mutation during operation",
            "No implicit timing exceptions or state-dependent path cuts; strict event separation = " + std::to_string(clock.event_separation_ticks)};
        try {
            if (!root || clock.clock_pin.empty() || !clock.event_separation_ticks
                || !clock.maximum_nodes || !clock.maximum_edges || !clock.maximum_hierarchy_depth
                || !clock.maximum_state_cases || !clock.maximum_state_evaluations)
                throw std::runtime_error("Invalid safe-clock specification");
            collect(root);
            if (!unknown) { bindContracts(); bindBoundary();validateBinaryStartup(); }
            if (!unknown) propagate();
            if (!unknown && clock.binary_startup && !state_timing_checked)
                diagnostic("Binary-startup preservation proof unavailable; no unrestricted fallback certificate");
            if (!unknown && !state_timing_checked) checkTiming();
            if (!unknown && !violation) {
                if (period % 2) period = add(period, 1);
                report.status = ClockTimingStatus::SafeUnderContracts; report.safe_period_ticks = period;
                report.binary_startup_verified=clock.binary_startup&&state_timing_checked;
            } else if (!unknown) report.status = ClockTimingStatus::TimingViolation;
        } catch (const std::exception& e) { diagnostic(e.what()); }
        report.node_count = nodes.size();
        std::sort(report.contracts.begin(), report.contracts.end());
        report.contracts.erase(std::unique(report.contracts.begin(), report.contracts.end()), report.contracts.end());
        return report;
    }
};
} // namespace

const char* clockTimingStatusName(ClockTimingStatus status) {
    switch (status) {
    case ClockTimingStatus::SafeUnderContracts: return "safe-under-contracts";
    case ClockTimingStatus::TimingViolation: return "timing-violation";
    case ClockTimingStatus::Unknown: return "unknown";
    }
    return "unknown";
}
ClockTimingReport CircuitTimingAnalyzer::safeClock(const std::shared_ptr<Component>& root,
    const ClockSpecification& clock, const TimingContracts& contracts) {
    return Analysis(root, clock, contracts).run();
}
} // namespace circuit::analysis
