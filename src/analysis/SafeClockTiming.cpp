#include "analysis/SafeClockTiming.hpp"
#include "TimingCircuit.hpp"
#include "MemoryTimingState.hpp"
#include "TimedLogicDomain.hpp"
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

// ---- Memory-mode timing: data kept while the modes are examined ----------
struct Incoming { size_t from; DelayBounds delay; };
// What is known about one pin in one memory mode.
struct Bound {
    size_t settled = none, previous = none;
    std::vector<detail::TimedBit> bits;
    std::optional<uint64_t> value;
};
struct EndpointOutput {
    bool full;
    std::string name;
    size_t delay;
    size_t input = none;
    std::shared_ptr<BehavioralMemoryEndpoint> endpoint;
};
// Where one output bit of a Rewire, splitter or joiner comes from.
struct BitSource { size_t node = none, bit = 0, delay = 0; unsigned constant = 0; };
using BitRef = std::pair<size_t, size_t>; // node, bit
// The same for every mode of one analysis.
struct StateTiming {
    std::vector<detail::MemoryTimingGroup> groups;
    size_t case_count = 1;
    std::vector<std::vector<Incoming>> incoming;
    std::vector<Node> initial;
    std::vector<size_t> early, early_previous;
    std::map<size_t, std::vector<BitSource>> routing;
    std::map<size_t, std::shared_ptr<IOComponent>> primitive;
    std::map<std::tuple<BitRef, unsigned, BitRef>, size_t> clamp_cache;
    size_t controlling_proofs = 0;
};
// One mode: one choice of empty/held state for every memory endpoint.
struct ModeState {
    std::vector<Bound> bounds;
    std::map<size_t, EndpointOutput> endpoint_outputs;
    std::set<size_t> forced;
};
bool isGate(const IOComponent& c) {
    return typeid(c) == typeid(NOTGate) || typeid(c) == typeid(ANDGate) || typeid(c) == typeid(NANDGate)
        || typeid(c) == typeid(ORGate) || typeid(c) == typeid(NORGate) || typeid(c) == typeid(XORGate);
}
// Four-state gate result over the codes 0 = LOW, 1 = HIGH, 2 = X, 3 = Z.
unsigned gateValue(const IOComponent& gate, unsigned a, unsigned b) {
    const bool inverted = typeid(gate) == typeid(NANDGate) || typeid(gate) == typeid(NORGate);
    unsigned v = 2;
    if (typeid(gate) == typeid(NOTGate)) v = a < 2 ? 1 - a : 2;
    else if (typeid(gate) == typeid(ANDGate) || typeid(gate) == typeid(NANDGate)) v = !a || !b ? 0 : a == 1 && b == 1 ? 1 : 2;
    else if (typeid(gate) == typeid(ORGate) || typeid(gate) == typeid(NORGate)) v = a == 1 || b == 1 ? 1 : !a && !b ? 0 : 2;
    else if (typeid(gate) == typeid(XORGate)) v = a < 2 && b < 2 ? (a != b) : 2;
    return inverted && v < 2 ? 1 - v : v;
}

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

    // Earliest transitions retain EVERY all-state arc, including feedback.
    // Only upper settling bounds use mode information. A shortest-path
    // search works with cycles and must not discard a hold-time hazard.
    void earliestArrivals(StateTiming& work) {
        work.incoming.assign(nodes.size(), {});
        for (size_t n = 0; n < nodes.size(); ++n)
            for (const auto& e : edges[n]) work.incoming[e.to].push_back({n, e.delay});
        work.initial = nodes;
        work.early.assign(nodes.size(), none);
        work.early_previous.assign(nodes.size(), none);
        using Entry = std::pair<size_t, size_t>;
        std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> heap;
        for (size_t n = 0; n < nodes.size(); ++n) if (nodes[n].arrival[0].valid) {
            work.early[n] = nodes[n].arrival[0].early;
            heap.emplace(work.early[n], n);
        }
        while (!heap.empty()) {
            const auto [time, n] = heap.top();
            heap.pop();
            if (time != work.early[n]) continue;
            for (const auto& e : edges[n]) {
                const auto t = add(time, e.delay.minimum);
                if (t < work.early[e.to]) {
                    work.early[e.to] = t;
                    work.early_previous[e.to] = n;
                    heap.emplace(t, e.to);
                }
            }
        }
    }

    // Which input bit each output bit of the routing parts carries.
    void buildBitRouting(StateTiming& work) {
        for (const auto& leaf : leaves) {
            const auto& c = *leaf.component;
            for (const auto& [name, p] : c.getAllOutputPins()) {
                const auto n = indices.at(p);
                work.primitive[n] = leaf.component;
                if (typeid(c) == typeid(Rewire)) {
                    const auto& rw = static_cast<const Rewire&>(c);
                    auto& r = work.routing[n];
                    r.resize(p->getWidth());
                    const auto value = rw.getUnmappedDefault() == Rewire::UnmappedBitValue::LOW ? 1u
                        : rw.getUnmappedDefault() == Rewire::UnmappedBitValue::HIGH ? 2u : 4u;
                    for (auto& bit : r) bit.constant = value;
                    for (const auto& bit : rw.getBitMappings()) if (bit.dst_wire == name)
                        r[bit.dst_bit] = {indices.at(c.getInputPinDynamic(bit.src_wire)), bit.src_bit, 0, 0};
                } else if (detail::isBitSplitter(c)) {
                    work.routing[n] = {{indices.at(c.getInputPinDynamic("IN")), std::stoul(name.substr(4)),
                                        static_cast<const BasicComponent&>(c).getDelay(), 0}};
                } else if (detail::isBitJoiner(c)) {
                    auto& r = work.routing[n];
                    for (size_t bit = 0; bit < p->getWidth(); ++bit)
                        r.push_back({indices.at(c.getInputPin("IN_" + std::to_string(bit))), 0,
                                     static_cast<const BasicComponent&>(c).getDelay(), 0});
                }
            }
        }
    }

    // Follow pure routing back to the bit a signal really comes from.
    BitRef origin(const StateTiming& work, BitRef p) const {
        std::set<BitRef> seen;
        while (seen.insert(p).second) {
            if (auto r = work.routing.find(p.first); r != work.routing.end()) {
                const auto& s = r->second.at(p.second);
                if (s.node == none) break;
                p = {s.node, s.bit};
            } else if (!work.primitive.contains(p.first) && work.incoming[p.first].size() == 1)
                p.first = work.incoming[p.first][0].from;
            else break;
        }
        return p;
    }

    // Prove a controlling-value implication from actual gates, including
    // its delay: if `seed` holds `value`, by when does `target` hold it too?
    // No CPU names or false-path lists enter this proof. Cycles, unsupported
    // primitives and depth exhaustion give no implication (`none`).
    size_t clampDelay(StateTiming& work, BitRef seed, unsigned value, BitRef target) {
        const auto query = std::make_tuple(seed, value, target);
        if (auto it = work.clamp_cache.find(query); it != work.clamp_cache.end()) return it->second;
        using Key = std::tuple<size_t, size_t, unsigned>;
        std::map<Key, size_t> memo;
        std::set<Key> active;
        auto visit = [&](auto&& self, BitRef p, unsigned want, size_t depth) -> size_t {
            if (report.state_evaluations >= clock.maximum_state_evaluations)
                throw std::runtime_error("State implication work budget exhausted");
            ++report.state_evaluations;
            if (p == seed) return want == value ? 0 : none;
            const Key key{p.first, p.second, want};
            if (auto it = memo.find(key); it != memo.end()) return it->second;
            if (depth >= 128 || !active.insert(key).second) return none;
            size_t time = none;
            if (auto r = work.routing.find(p.first); r != work.routing.end()) {
                const auto& s = r->second.at(p.second);
                if (s.node == none) {
                    if (s.constant == (1u << want)) time = 0;
                } else {
                    time = self(self, {s.node, s.bit}, want, depth + 1);
                    if (time != none) time = add(time, s.delay);
                }
            } else if (!work.primitive.contains(p.first) && work.incoming[p.first].size() == 1) {
                const auto& e = work.incoming[p.first][0];
                time = self(self, {e.from, p.second}, want, depth + 1);
                if (time != none) time = add(time, e.delay.maximum);
            } else if (auto it = work.primitive.find(p.first); it != work.primitive.end()) {
                const auto& c = *it->second;
                const bool and_gate = typeid(c) == typeid(ANDGate) || typeid(c) == typeid(NANDGate);
                const bool or_gate = typeid(c) == typeid(ORGate) || typeid(c) == typeid(NORGate);
                if (typeid(c) == typeid(NOTGate))
                    time = self(self, {indices.at(c.getInputPin("IN")), 0}, 1 - want, depth + 1);
                else if (and_gate || or_gate) {
                    const bool inverted = typeid(c) == typeid(NANDGate) || typeid(c) == typeid(NORGate);
                    const unsigned input_value = inverted ? 1 - want : want;
                    const auto a = self(self, {indices.at(c.getInputPin("A")), 0}, input_value, depth + 1);
                    const auto b = self(self, {indices.at(c.getInputPin("B")), 0}, input_value, depth + 1);
                    time = (and_gate ? input_value == 0 : input_value == 1) ? std::min(a, b) : std::max(a, b);
                }
                if (time != none) time = add(time, static_cast<const BasicComponent&>(c).getDelay());
            }
            active.erase(key);
            memo[key] = time;
            return time;
        };
        const auto result = visit(visit, target, value, 0);
        work.clamp_cache[query] = result;
        return result;
    }

    // What is known before anything propagates in mode `mode`.
    // False: the circuit does not fit the mode model; the general analysis applies.
    bool startMode(const StateTiming& work, size_t mode, ModeState& state) {
        const auto& initial = work.initial;
        auto& bounds = state.bounds;
        bounds.assign(nodes.size(), {});
        for (size_t n = 0; n < nodes.size(); ++n) {
            bounds[n].bits.resize(nodes[n].pin->getWidth());
            if (initial[n].stable) bounds[n].settled = 0;
            if (initial[n].value) bounds[n].value = initial[n].value;
            if (work.incoming[n].empty() && initial[n].arrival[0].valid) bounds[n].settled = initial[n].arrival[0].late;
            for (size_t i = 0; i < bounds[n].bits.size(); ++i) {
                auto& bit = bounds[n].bits[i];
                bit.settled = bounds[n].settled;
                if (initial[n].value) bit.values = 1u << ((*initial[n].value >> i) & 1);
                else if (clock.binary_startup && work.incoming[n].empty() && initial[n].arrival[0].valid)
                    bit = {3, initial[n].arrival[0].late, bit.settled};
            }
        }
        if (clock.binary_startup) for (const auto& g : work.groups) {
            // With binary cells and saved data, FF responses are binary
            // even when an invalid transient request contains X/Z: it
            // returns a binary error code and zero payload. Preservation
            // of all write/response captures is checked below.
            for (const auto& [name, p] : g.endpoint->getAllOutputPins())
                for (auto& bit : bounds[indices.at(p)].bits) { bit.values = 3; bit.at = g.endpoint->getDelay(); }
        }
        size_t selection = mode;
        for (const auto& g : work.groups) {
            const auto& c = g.cases[selection % g.cases.size()];
            selection /= g.cases.size();
            for (const auto& [pin, value] : c.registers) {
                const auto n = indices.at(pin);
                if (!initial[n].arrival[0].valid || !work.incoming[n].empty()) return false;
                bounds[n].bits[0] = {1u << unsigned(value), bounds[n].settled, bounds[n].settled};
                if (value == LogicValue::LOW || value == LogicValue::HIGH) bounds[n].value = unsigned(value);
            }
            for (const auto& [name, p] : g.endpoint->getAllOutputPins()) {
                const auto n = indices.at(p);
                const auto d = g.endpoint->getDelay();
                EndpointOutput e{c.full, name, d, none, g.endpoint};
                if (name == "REQ_READY") e.input = indices.at(g.endpoint->getInputPin("RSP_READY"));
                if (name == "RSP_VALID" || name == "RSP_READ_DATA") e.input = indices.at(g.endpoint->getInputPin("REQ_VALID"));
                state.endpoint_outputs[n] = e;
                const bool fixed_one = (name == "REQ_READY" && !c.full) || (name == "RSP_VALID" && c.full);
                if (fixed_one || (c.full && name != "REQ_READY")) {
                    // New mode is selected at the edge; old output events
                    // have drained under the previous cycle's bound. The
                    // published output need not be stable until edge+d.
                    bounds[n].settled = d;
                    state.forced.insert(n);
                    for (auto& bit : bounds[n].bits) bit.settled = d;
                    if (fixed_one) { bounds[n].bits[0] = {2, d, d}; bounds[n].value = 1; }
                }
            }
        }
        return true;
    }

    // Tightens what is known about one pin, remembering whether anything moved.
    struct Refiner {
        Bound& b;
        bool changed = false;
        void settle(size_t t, size_t prev) {
            if (t < b.settled) { b.settled = t; b.previous = prev; changed = true; }
            for (auto& bit : b.bits) if (t < bit.settled) { bit.settled = t; changed = true; }
        }
        void refine(size_t i, const detail::TimedBit& proof) {
            auto& bit = b.bits.at(i);
            const auto narrowed = bit.values & proof.values;
            if (!narrowed) throw std::runtime_error("Contradictory state timing domains");
            if (narrowed != bit.values) { bit.values = narrowed; bit.at = std::max(bit.at, proof.at); changed = true; }
            else if (proof.values == bit.values && proof.at < bit.at) { bit.at = proof.at; changed = true; }
            size_t time = proof.settled;
            if ((bit.values & (bit.values - 1)) == 0) time = std::min(time, bit.at);
            if (time < bit.settled) { bit.settled = time; changed = true; }
        }
    };

    void refineEndpointOutput(const EndpointOutput& e, ModeState& state, Refiner& r) {
        auto& bounds = state.bounds;
        if (e.name == "RSP_STATUS" && !e.full) {
            auto input = [&](const char* name) -> const std::vector<detail::TimedBit>& {
                return bounds[indices.at(e.endpoint->getInputPinDynamic(name))].bits;
            };
            const auto& storage = e.endpoint->memoryStorage();
            const auto proof = detail::ffStatusBounds(input("REQ_OP"), input("REQ_ADDR"), input("REQ_WRITE_MASK"),
                storage.wordBits() / 8, storage.capacityBytes(), e.delay);
            for (size_t i = 0; i < 3; ++i) r.refine(i, proof[i]);
        } else if (e.input != none && (e.name == "REQ_READY" || e.name == "RSP_VALID")) {
            const auto& input = bounds[e.input].bits[0];
            unsigned domain = 0;
            for (unsigned v = 0; v < 4; ++v) if (input.values & (1u << v)) domain |= v == 1 ? 2 : 1;
            r.refine(0, {domain, add(input.at, e.delay), none});
        } else if (e.input != none && e.name == "RSP_READ_DATA" && !(bounds[e.input].bits[0].values & 2)) {
            r.settle(add(bounds[e.input].bits[0].at, e.delay), e.input);
            r.b.value = 0;
            for (size_t i = 0; i < r.b.bits.size(); ++i) r.refine(i, {1, r.b.settled, r.b.settled});
        }
    }

    void refinePrimitiveOutput(const IOComponent& c, size_t n, bool implication_pass,
                               StateTiming& work, ModeState& state, Refiner& r) {
        auto& bounds = state.bounds;
        const auto delay = [&] { return static_cast<const BasicComponent&>(c).getDelay(); };
        if (clock.binary_startup && typeid(c) == typeid(RegisterFile32x32Behavioral)) {
            const auto addr = nodes[n].pin->getName() == "RS1_DATA" ? "RS1_ADDR" : "RS2_ADDR";
            size_t at = work.initial[n].arrival[0].late;
            bool binary = true;
            for (const auto& bit : bounds[indices.at(c.getInputPin<5>(addr))].bits) {
                binary &= !(bit.values & 12);
                at = std::max(at, add(bit.at, delay()));
            }
            if (binary) for (size_t i = 0; i < r.b.bits.size(); ++i) r.refine(i, {3, at, none});
        }
        if (!isGate(c)) return;
        const bool inverter = typeid(c) == typeid(NOTGate);
        const auto& a = bounds[indices.at(c.getInputPin(inverter ? "IN" : "A"))].bits[0];
        const auto& other = inverter ? a : bounds[indices.at(c.getInputPin("B"))].bits[0];
        unsigned domain = 0;
        for (unsigned x = 0; x < 4; ++x) for (unsigned y = 0; y < 4; ++y)
            if ((a.values & (1u << x)) && (other.values & (1u << y))) domain |= 1u << gateValue(c, x, y);
        r.refine(0, {domain, add(std::max(a.at, other.at), delay()), none});
        if (!implication_pass || r.b.settled != none || (typeid(c) != typeid(ANDGate) && typeid(c) != typeid(ORGate)))
            return;
        for (const auto* input : {"A", "B"}) {
            const auto p = indices.at(c.getInputPin(input));
            const auto q = indices.at(c.getInputPin(input[0] == 'A' ? "B" : "A"));
            const auto source = origin(work, {p, 0});
            const auto& s = bounds[source.first].bits[source.second];
            if (s.settled == none || s.values != 3 || bounds[p].bits[0].settled == none) continue;
            // OR(s, s & ...) = s; AND(s, s | ...) = s.
            // The unbounded input may glitch until its
            // controlling value has propagated to it.
            const auto d = clampDelay(work, source, typeid(c) == typeid(ORGate) ? 0 : 1, {q, 0});
            if (d == none) continue;
            const auto at = add(std::max(bounds[p].bits[0].settled, add(s.settled, d)), delay());
            r.settle(at, source.first);
            r.refine(0, {s.values, at, at});
            ++work.controlling_proofs;
        }
    }

    // One step of the fixpoint for pin `n`. True: something about it tightened.
    bool refineNode(size_t n, bool implication_pass, StateTiming& work, ModeState& state) {
        auto& bounds = state.bounds;
        Refiner r{bounds[n]};
        const auto& initial = work.initial;
        const auto& incoming = work.incoming[n];
        // Generic dependency rule: all inputs settled implies output
        // settled. Including launch time accounts for changing state.
        size_t latest = initial[n].arrival[0].valid ? initial[n].arrival[0].late : 0, prev = none;
        bool complete = !incoming.empty();
        for (const auto& e : incoming) {
            if (bounds[e.from].settled == none) { complete = false; break; }
            const auto t = add(bounds[e.from].settled, e.delay.maximum);
            if (t > latest) { latest = t; prev = e.from; }
        }
        if (complete) r.settle(latest, prev);

        if (const auto ep = state.endpoint_outputs.find(n); ep != state.endpoint_outputs.end()) {
            refineEndpointOutput(ep->second, state, r);
        } else if (auto it = work.routing.find(n); it != work.routing.end()) {
            for (size_t i = 0; i < it->second.size(); ++i) {
                const auto& route = it->second[i];
                if (route.node == none) { r.refine(i, {route.constant, 0, 0}); continue; }
                auto proof = bounds[route.node].bits.at(route.bit);
                proof.at = add(proof.at, route.delay);
                if (proof.settled != none) proof.settled = add(proof.settled, route.delay);
                r.refine(i, proof);
            }
        } else if (auto it = work.primitive.find(n); it != work.primitive.end()) {
            refinePrimitiveOutput(*it->second, n, implication_pass, work, state, r);
        } else if (incoming.size() == 1) {
            const auto& from = bounds[incoming[0].from];
            for (size_t i = 0; i < r.b.bits.size(); ++i) r.refine(i, from.bits.at(i));
            if (from.value && r.b.value != from.value) { r.b.value = from.value; r.changed = true; }
        }
        const auto bit_time = detail::settledAt(r.b.bits);
        if (bit_time < r.b.settled) { r.b.settled = bit_time; r.b.previous = none; r.changed = true; }
        return r.changed;
    }

    // Propagate until nothing tightens; then one more round in which the
    // gate-implication proof may settle what is still open.
    // False: the work budget ran out.
    bool settleMode(StateTiming& work, ModeState& state) {
        std::deque<size_t> pending;
        std::vector<bool> queued(nodes.size(), true);
        for (size_t n = 0; n < nodes.size(); ++n) pending.push_back(n);
        bool implication_pass = false;
        while (!pending.empty() || !implication_pass) {
            if (pending.empty()) {
                implication_pass = true;
                for (size_t n = 0; n < nodes.size(); ++n)
                    if (state.bounds[n].settled == none && !state.forced.contains(n)) { pending.push_back(n); queued[n] = true; }
                if (pending.empty()) break;
            }
            if (report.state_evaluations == clock.maximum_state_evaluations) return false;
            ++report.state_evaluations;
            const auto n = pending.front();
            pending.pop_front();
            queued[n] = false;
            if (state.forced.contains(n)) continue;
            if (!refineNode(n, implication_pass, work, state)) continue;
            for (const auto& e : edges[n]) if (!queued[e.to]) { queued[e.to] = true; pending.push_back(e.to); }
        }
        return true;
    }

    bool modeSettled(const ModeState& state) const {
        for (size_t n = 0; n < nodes.size(); ++n)
            if (n != clock_node && !clock_nodes.contains(n) && !unused.contains(n) && state.bounds[n].settled == none)
                return false;
        return true;
    }

    void explainUnsettledMode(const StateTiming& work, const ModeState& state, size_t mode) {
        for (const auto& g : work.groups) {
            const auto n = indices.at(g.endpoint->getOutputPin<3>("RSP_STATUS"));
            if (state.bounds[n].settled != none) continue;
            for (const auto* name : {"REQ_OP", "REQ_ADDR", "REQ_WRITE_MASK"}) {
                const auto& input = state.bounds[indices.at(g.endpoint->getInputPinDynamic(name))];
                std::string domains;
                for (const auto& bit : input.bits) domains += "0123456789abcdef"[bit.values];
                report.state_evidence.push_back(g.endpoint->getID() + "." + name + ": bit domains (LSB first)=" + domains
                    + " (hex sets: 1=LOW, 2=HIGH, 4=X, 8=Z), settles="
                    + (input.settled == none ? "unproved" : std::to_string(input.settled)));
            }
        }
        report.state_evidence.push_back("State case " + std::to_string(mode)
            + ": no bounded settling proof; retaining all-state analysis");
    }

    // True: the analysis finished here, with a result or a diagnostic.
    // False: the general analysis in propagate() applies instead.
    bool propagateMemoryStates() {
        StateTiming work;
        if (const auto decided = collectMemoryGroups(work)) return *decided;
        recordStatePremises(work);
        earliestArrivals(work);
        buildBitRouting(work);
        for (size_t mode = 0; mode < work.case_count; ++mode) {
            ModeState state;
            if (!startMode(work, mode, state)) return false;
            if (!settleMode(work, state)) {
                nodes = work.initial;
                diagnostic("State settling work budget exhausted");
                return true;
            }
            if (!modeSettled(state)) {
                explainUnsettledMode(work, state, mode);
                nodes = work.initial;
                if (clock.binary_startup) { diagnostic("Binary-startup settling/preservation proof incomplete"); return true; }
                return false;
            }
            auto& bounds = state.bounds;
            if (clock.binary_startup) for (const auto& [n, cap] : captures)
                for (size_t bit = 0; bit < bounds[n].bits.size(); ++bit) {
                    const auto& proof = bounds[n].bits[bit];
                    if (proof.values & 12) {
                        diagnostic(nodes[n].pin->getID() + "[" + std::to_string(bit) + "]: binary next-state preservation unproved");
                        nodes = work.initial;
                        return true;
                    }
                    // Domain restrictions must arrive before capture too, not just
                    // the possibly earlier value-settling deadline.
                    bounds[n].settled = std::max(bounds[n].settled, proof.at);
                    ++report.binary_capture_bits;
                }
            nodes = work.initial;
            for (size_t n = 0; n < nodes.size(); ++n) {
                if (work.early[n] != none)
                    nodes[n].arrival[0] = {true, work.early[n], bounds[n].settled, bounds[n].previous, work.early_previous[n]};
                else { nodes[n].stable = true; nodes[n].value = bounds[n].value; }
            }
            ++report.checked_state_cases;
            checkTiming();
            if (unknown || violation) { state_timing_checked = true; return true; }
        }
        state_timing_checked = true;
        if (work.controlling_proofs) report.state_evidence.push_back(std::to_string(work.controlling_proofs)
            + " timed controlling-value implications proved from native gate wiring");
        report.assumptions.push_back(clock.binary_startup
            ? "All binary memory control cases and captured-bit preservation checked with timed settling; hold uses all uncut arcs"
            : "All reachable four-state memory control cases checked with timed settling; hold uses all uncut arcs");
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
        if(propagateMemoryStates())return;
        std::deque<size_t> queue;
        for (size_t i = 0; i < nodes.size(); ++i) if (!indegree[i]) queue.push_back(i);
        size_t processed = 0;
        while (!queue.empty()) {
            const auto a = queue.front(); queue.pop_front(); ++processed;
            for (const auto& e : edges[a]) {
                for (size_t k = 0; k < 2; ++k) {
                    const auto from = nodes[a].arrival[k]; if (!from.valid) continue;
                    auto& to = nodes[e.to].arrival[k];
                    const auto early = add(from.early, e.delay.minimum), late = add(from.late, e.delay.maximum);
                    if (!to.valid || early < to.early) { to.early = early; to.early_previous = a; }
                    if (!to.valid || late > to.late) { to.late = late; to.previous = a; }
                    to.valid = true;
                }
                if (e.wire && nodes[a].value) nodes[e.to].value = nodes[a].value;
                if (--indegree[e.to] == 0) {
                    // Unchanging combinational inputs imply unchanging output
                    // after initialization; this is not constant-value folding.
                    if (!nodes[e.to].arrival[0].valid && !nodes[e.to].arrival[1].valid) nodes[e.to].stable = true;
                    queue.push_back(e.to);
                }
            }
        }
        if (processed != nodes.size()) {
            diagnostic("Combinational dependency cycle: no finite all-state settling bound established");
            // Kahn's remainder contains downstream pins as well as cycle pins.
            // Find a real closed walk instead of mislabeling arbitrary captures
            // as the cause. Iterative DFS keeps large gate graphs off the stack.
            std::vector<unsigned char> color(nodes.size());
            std::vector<std::pair<size_t,size_t>> stack;
            for (size_t start=0; start<nodes.size() && report.dependency_cycle.empty(); ++start) {
                if (!indegree[start] || color[start]) continue;
                stack.emplace_back(start,0); color[start]=1;
                while (!stack.empty() && report.dependency_cycle.empty()) {
                    auto& [n,next]=stack.back();
                    if (next==edges[n].size()) {color[n]=2;stack.pop_back();continue;}
                    const auto target=edges[n][next++].to;
                    if (!indegree[target] || color[target]==2) continue;
                    if (!color[target]) {color[target]=1;stack.emplace_back(target,0);continue;}
                    auto begin=std::find_if(stack.begin(),stack.end(),[&](const auto& frame){return frame.first==target;});
                    for (auto it=begin;it!=stack.end();++it) report.dependency_cycle.push_back(nodes[it->first].pin->getID());
                    report.dependency_cycle.push_back(nodes[target].pin->getID());
                }
            }
        }
    }
    void path(size_t n, size_t k, bool earliest = false) {
        report.critical_path.clear();
        while (n != none) {
            report.critical_path.push_back(nodes[n].pin->getID());
            n = earliest ? nodes[n].arrival[k].early_previous : nodes[n].arrival[k].previous;
        }
        std::reverse(report.critical_path.begin(), report.critical_path.end());
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
ClockTimingReport computeSafeClockPeriod(const std::shared_ptr<Component>& root,
    const ClockSpecification& clock, const TimingContracts& contracts) {
    return Analysis(root, clock, contracts).run();
}
} // namespace circuit::analysis
