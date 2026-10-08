#include "analysis/SettlingBound.hpp"
#include "BuiltinClockContracts.hpp"
#include "basic/PinBase.hpp"
#include "basic/WireBase.hpp"
#include "components/BasicComponent.hpp"
#include "components/IOComponent.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Rewire.hpp"
#include <algorithm>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>

namespace circuit::analysis {
namespace {

// "Never changes during operation". Smaller than every tick, so max() ignores
// it and min() prefers it.
constexpr long long STABLE = -1;
constexpr int8_t UNKNOWN = 2;

enum class Kind : uint8_t { Stable, Source, Buf, Not, And, Nand, Or, Nor, Xor, Hub };

struct Node {
    Kind kind = Kind::Stable;
    int8_t constant = UNKNOWN; // Stable: known final value, if any
    int a = -1, b = -1;
    long long delay = 0;       // Source: tick of the last transition, or STABLE
    int var = -1;              // Source: case-split variable
    int hub = -1;
    const PinBase* pin = nullptr;
    unsigned bit = 0;
};
// An opaque contract output: latest of its launch and its timed input arcs.
struct Hub {
    std::vector<std::pair<int, long long>> inputs;
    long long launch = STABLE;
};
struct Leaf {
    std::shared_ptr<IOComponent> component;
    ClockComponentContract contract;
    bool native = false;
};
struct Group {
    std::string name;
    std::vector<std::pair<int, long long>> endpoints; // node, added setup
};

template<size_t W> bool splitterOf(const IOComponent& c) { return typeid(c) == typeid(BitSplitter<W>); }
template<size_t W> bool joinerOf(const IOComponent& c) { return typeid(c) == typeid(BitJoiner<W>); }
bool isSplitter(const IOComponent& c) {
    return splitterOf<1>(c) || splitterOf<2>(c) || splitterOf<3>(c) || splitterOf<4>(c)
        || splitterOf<5>(c) || splitterOf<8>(c) || splitterOf<16>(c) || splitterOf<32>(c);
}
bool isJoiner(const IOComponent& c) {
    return joinerOf<1>(c) || joinerOf<2>(c) || joinerOf<3>(c) || joinerOf<4>(c)
        || joinerOf<5>(c) || joinerOf<8>(c) || joinerOf<16>(c) || joinerOf<32>(c);
}
std::optional<Kind> gateKind(const IOComponent& c) {
    if (typeid(c) == typeid(NOTGate)) return Kind::Not;
    if (typeid(c) == typeid(ANDGate)) return Kind::And;
    if (typeid(c) == typeid(NANDGate)) return Kind::Nand;
    if (typeid(c) == typeid(ORGate)) return Kind::Or;
    if (typeid(c) == typeid(NORGate)) return Kind::Nor;
    if (typeid(c) == typeid(XORGate)) return Kind::Xor;
    return std::nullopt;
}

class Analysis {
    const std::shared_ptr<Component>& root_;
    const SettlingOptions& options_;
    SettlingReport report_;
    std::shared_ptr<IOComponent> root_io_;
    bool root_is_leaf_ = false, failed_ = false;

    std::vector<Leaf> leaves_;
    std::vector<Node> nodes_;
    std::vector<Hub> hubs_;
    std::unordered_map<const PinBase*, int> base_;
    std::unordered_map<const PinBase*, std::shared_ptr<PinBase>> driver_;
    std::unordered_set<const PinBase*> clock_roots_, clock_sinks_;
    std::vector<std::pair<const PinBase*, unsigned>> variables_;
    std::vector<Group> groups_;
    std::vector<std::tuple<int, int8_t, std::string>> required_;
    std::vector<int> order_;

    std::vector<int8_t> value_;
    std::vector<long long> time_;
    std::vector<int> previous_, stamp_;
    int stamp_counter_ = 0;
    size_t evaluations_ = 0;

    // The case being examined: final values assumed for some bits.
    std::vector<int8_t> forced_;
    std::vector<uint8_t> binary_;      // final value is certainly 0 or 1
    std::vector<int> touched_, unjustified_, variable_node_;
    std::vector<double> weight_;
    bool conflict_ = false;

    void fail(const std::string& message) {
        failed_ = true;
        if (report_.diagnostics.size() < 64) report_.diagnostics.push_back(message);
    }
    bool rootPin(const PinBase& pin) const { return pin.getOwner() == root_; }
    int reserve(const std::shared_ptr<PinBase>& pin) {
        const auto [it, added] = base_.emplace(pin.get(), static_cast<int>(nodes_.size()));
        if (added) for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
            nodes_.emplace_back();
            nodes_.back().pin = pin.get();
            nodes_.back().bit = bit;
        }
        return it->second;
    }
    Node& at(const std::shared_ptr<PinBase>& pin, size_t bit) { return nodes_[base_.at(pin.get()) + bit]; }
    int index(const std::shared_ptr<PinBase>& pin, size_t bit) const {
        return base_.at(pin.get()) + static_cast<int>(bit);
    }

    void collect(const std::shared_ptr<Component>& component, size_t depth = 0) {
        if (depth > 256) throw std::runtime_error("Hierarchy depth limit exceeded");
        if (auto io = std::dynamic_pointer_cast<IOComponent>(component)) {
            auto contract = options_.contracts.model ? options_.contracts.model(*io) : std::nullopt;
            const bool native = !contract;
            if (!contract) contract = detail::builtinClockContract(*io);
            if (contract) {
                if (component == root_) root_is_leaf_ = true;
                leaves_.push_back({io, std::move(*contract), native});
                return;
            }
        }
        if (std::dynamic_pointer_cast<BasicComponent>(component) || component->getChildren().empty()) {
            fail(component->getID() + ": no timing contract for " + component->getTypeName());
            return;
        }
        for (const auto& child : component->getChildren()) collect(child, depth + 1);
    }

    // Follow one driven net through hierarchy boundaries to the pins that read it.
    void trace(const std::shared_ptr<PinBase>& source, const std::shared_ptr<WireBase>& first) {
        std::vector<std::shared_ptr<WireBase>> pending{first};
        std::unordered_set<const WireBase*> seen;
        while (!pending.empty()) {
            const auto wire = pending.back();
            pending.pop_back();
            if (!wire || !seen.insert(wire.get()).second) continue;
            for (const auto& weak : wire->getSinkPinsBase()) {
                const auto sink = weak.lock();
                if (!sink || sink == source) continue;
                if (base_.contains(sink.get())) {
                    const bool reads = rootPin(*sink) && !root_is_leaf_
                        ? sink->getType() == PinType::OUTPUT : sink->getType() == PinType::INPUT;
                    if (!reads) continue;
                    if (sink->getWidth() != source->getWidth())
                        throw std::runtime_error("Width mismatch at " + sink->getID());
                    const auto [it, added] = driver_.emplace(sink.get(), source);
                    if (!added && it->second != source)
                        throw std::runtime_error("Two drivers for " + sink->getID());
                    continue;
                }
                pending.push_back(sink->getType() == PinType::INPUT
                    ? sink->getInternalWireBase() : sink->getExternalWireBase());
            }
        }
    }

    void makeSource(const std::shared_ptr<PinBase>& pin, long long arrival, bool splittable) {
        for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
            auto& node = at(pin, bit);
            node.kind = Kind::Source;
            node.delay = arrival;
            if (splittable && options_.binary_sources) {
                node.var = static_cast<int>(variables_.size());
                variables_.emplace_back(pin.get(), bit);
                variable_node_.push_back(index(pin, bit));
            }
        }
    }
    void makeConstant(const std::shared_ptr<PinBase>& pin, uint64_t value) {
        if (pin->getWidth() < 64 && value >= (uint64_t{1} << pin->getWidth()))
            throw std::runtime_error("Value exceeds pin width: " + pin->getID());
        for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
            auto& node = at(pin, bit);
            node.kind = Kind::Stable;
            node.constant = static_cast<int8_t>((value >> bit) & 1);
        }
    }
    void makeRootInput(const std::shared_ptr<PinBase>& pin) {
        if (clock_roots_.contains(pin.get())) return; // not data
        const auto name = pin->getName();
        if (const auto fixed = options_.fixed_inputs.find(name); fixed != options_.fixed_inputs.end()) {
            makeConstant(pin, fixed->second);
            return;
        }
        long long arrival = 0;
        if (const auto it = options_.input_arrival.find(name); it != options_.input_arrival.end())
            arrival = static_cast<long long>(it->second);
        const auto final_value = options_.final_inputs.find(name);
        makeSource(pin, options_.sources == SettlingSources::StateOnly ? STABLE : arrival,
                   final_value == options_.final_inputs.end());
        if (final_value == options_.final_inputs.end()) return;
        if (pin->getWidth() < 64 && final_value->second >= (uint64_t{1} << pin->getWidth()))
            throw std::runtime_error("Value exceeds pin width: " + pin->getID());
        for (unsigned bit = 0; bit < pin->getWidth(); ++bit)
            at(pin, bit).constant = static_cast<int8_t>((final_value->second >> bit) & 1);
    }

    void makeLeaf(const Leaf& leaf) {
        const auto& c = *leaf.component;
        const auto& m = leaf.contract;
        const auto own = [&](const std::string& name, PinType type) {
            auto pin = type == PinType::INPUT ? c.getInputPinDynamic(name) : c.getOutputPinDynamic(name);
            if (!pin) throw std::runtime_error(c.getID() + ": contract names missing pin " + name);
            return pin;
        };
        std::set<std::string> classified{m.clock_pin};
        for (const auto& arc : m.arcs) classified.insert(arc.input);
        for (const auto& capture : m.captures) {
            if (capture.edge != ClockEdge::Rising) fail(c.getID() + ": falling-edge capture is unsupported");
            classified.insert(capture.pin);
        }
        for (const auto& [name, value] : m.fixed_inputs) classified.insert(name);
        for (const auto& name : m.unused_inputs) classified.insert(name);

        for (const auto& [name, pin] : c.getAllInputPins()) {
            if (!classified.contains(name)) fail(pin->getID() + ": input absent from timing contract");
            if (name == m.clock_pin) continue;
            if (leaf.component == root_) { makeRootInput(pin); continue; }
            const auto source = driver_.find(pin.get());
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
                auto& node = at(pin, bit);
                if (source == driver_.end()) continue; // undriven: never changes, value unknown
                node.kind = Kind::Buf;
                node.a = index(source->second, bit);
            }
        }
        for (const auto& capture : m.captures) {
            const auto pin = own(capture.pin, PinType::INPUT);
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit)
                groups_.front().endpoints.emplace_back(index(pin, bit), static_cast<long long>(capture.setup));
        }
        for (const auto& [name, value] : m.fixed_inputs) {
            const auto pin = own(name, PinType::INPUT);
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit)
                required_.emplace_back(index(pin, bit), static_cast<int8_t>((value >> bit) & 1), pin->getID());
        }

        const auto delay = [&]() {
            return static_cast<long long>(static_cast<const BasicComponent&>(c).getDelay());
        };
        if (leaf.native) {
            if (const auto kind = gateKind(c)) {
                auto& out = at(own("OUT", PinType::OUTPUT), 0);
                out.kind = *kind;
                out.delay = delay();
                if (*kind == Kind::Not) out.a = index(own("IN", PinType::INPUT), 0);
                else { out.a = index(own("A", PinType::INPUT), 0); out.b = index(own("B", PinType::INPUT), 0); }
                return;
            }
            if (typeid(c) == typeid(Rewire)) {
                const auto& rewire = static_cast<const Rewire&>(c);
                for (const auto& [name, pin] : c.getAllOutputPins())
                    for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
                        auto& node = at(pin, bit);
                        node.kind = Kind::Stable;
                        if (rewire.getUnmappedDefault() == Rewire::UnmappedBitValue::LOW) node.constant = 0;
                        if (rewire.getUnmappedDefault() == Rewire::UnmappedBitValue::HIGH) node.constant = 1;
                    }
                for (const auto& map : rewire.getBitMappings()) {
                    auto& node = at(own(map.dst_wire, PinType::OUTPUT), map.dst_bit);
                    node.kind = Kind::Buf;
                    node.constant = UNKNOWN;
                    node.a = index(own(map.src_wire, PinType::INPUT), map.src_bit);
                    node.delay = delay();
                }
                return;
            }
            if (isSplitter(c)) {
                const auto in = own("IN", PinType::INPUT);
                for (unsigned bit = 0; bit < in->getWidth(); ++bit) {
                    auto& node = at(own("OUT_" + std::to_string(bit), PinType::OUTPUT), 0);
                    node.kind = Kind::Buf; node.a = index(in, bit); node.delay = delay();
                }
                return;
            }
            if (isJoiner(c)) {
                const auto out = own("OUT", PinType::OUTPUT);
                for (unsigned bit = 0; bit < out->getWidth(); ++bit) {
                    auto& node = at(out, bit);
                    node.kind = Kind::Buf; node.delay = delay();
                    node.a = index(own("IN_" + std::to_string(bit), PinType::INPUT), 0);
                }
                return;
            }
        }
        // Any other contract is opaque: timed arcs and launches, no values.
        for (const auto& [name, pin] : c.getAllOutputPins()) {
            if (const auto constant = m.constant_outputs.find(name); constant != m.constant_outputs.end()) {
                makeConstant(pin, constant->second);
                continue;
            }
            if (std::find(m.stable_outputs.begin(), m.stable_outputs.end(), name) != m.stable_outputs.end())
                continue;
            Hub hub;
            bool launched = false;
            for (const auto& launch : m.launches) if (launch.pin == name) {
                if (launch.edge != ClockEdge::Rising) fail(c.getID() + ": falling-edge launch is unsupported");
                launched = true;
                if (options_.sources != SettlingSources::InputsOnly)
                    hub.launch = std::max(hub.launch, static_cast<long long>(launch.delay.maximum));
            }
            for (const auto& arc : m.arcs) if (arc.output == name) {
                const auto in = own(arc.input, PinType::INPUT);
                for (unsigned bit = 0; bit < in->getWidth(); ++bit)
                    hub.inputs.emplace_back(index(in, bit), static_cast<long long>(arc.delay.maximum));
            }
            if (!launched && hub.inputs.empty()) {
                fail(pin->getID() + ": output absent from timing contract");
                continue;
            }
            if (hub.inputs.empty()) { makeSource(pin, hub.launch, true); continue; }
            hubs_.push_back(std::move(hub));
            for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
                auto& node = at(pin, bit);
                node.kind = Kind::Hub;
                node.hub = static_cast<int>(hubs_.size()) - 1;
            }
        }
    }

    template<class Visit> void inputsOf(int n, Visit&& visit) const {
        const auto& node = nodes_[n];
        if (node.kind == Kind::Hub) { for (const auto& [in, d] : hubs_[node.hub].inputs) visit(in); return; }
        if (node.a >= 0) visit(node.a);
        if (node.b >= 0) visit(node.b);
    }

    bool sort() {
        std::vector<int> waiting(nodes_.size());
        std::vector<std::vector<int>> readers(nodes_.size());
        for (int n = 0; n < static_cast<int>(nodes_.size()); ++n)
            inputsOf(n, [&](int in) { ++waiting[n]; readers[in].push_back(n); });
        for (int n = 0; n < static_cast<int>(nodes_.size()); ++n) if (!waiting[n]) order_.push_back(n);
        for (size_t next = 0; next < order_.size(); ++next)
            for (const int reader : readers[order_[next]]) if (!--waiting[reader]) order_.push_back(reader);
        if (order_.size() == nodes_.size()) return true;
        fail("Combinational dependency cycle: no finite settling bound");
        for (int n = 0, shown = 0; n < static_cast<int>(nodes_.size()) && shown < 8; ++n) if (waiting[n]) {
            report_.diagnostics.push_back("cycle or downstream: " + label(n));
            ++shown;
        }
        return false;
    }

    std::string label(int n) const {
        return nodes_[n].pin->getID() + "[" + std::to_string(nodes_[n].bit) + "]";
    }

    // The timed-value rules. `values == false` ignores every value (per-bit
    // structural longest path). Otherwise the assumed final values of the
    // current case are used, and a contradiction with them is recorded.
    void run(const std::vector<int>& order, bool values) {
        ++evaluations_;
        conflict_ = false;
        unjustified_.clear();
        for (const int n : order) {
            const auto& x = nodes_[n];
            int8_t v = UNKNOWN;
            long long s = STABLE;
            int p = -1;
            switch (x.kind) {
            case Kind::Stable: v = x.constant; break;
            case Kind::Source: s = x.delay; v = x.constant; break;
            case Kind::Buf: case Kind::Not:
                v = value_[x.a];
                if (x.kind == Kind::Not && v != UNKNOWN) v = static_cast<int8_t>(1 - v);
                s = time_[x.a]; p = x.a;
                break;
            case Kind::And: case Kind::Nand: case Kind::Or: case Kind::Nor: {
                const int8_t controlling = x.kind == Kind::And || x.kind == Kind::Nand ? 0 : 1;
                const bool inverted = x.kind == Kind::Nand || x.kind == Kind::Nor;
                const int8_t va = value_[x.a], vb = value_[x.b];
                const bool ca = va == controlling, cb = vb == controlling;
                if (ca || cb) {
                    // Fixed as soon as ONE controlling input is fixed.
                    p = ca && (!cb || time_[x.a] <= time_[x.b]) ? x.a : x.b;
                    v = controlling;
                } else {
                    p = time_[x.a] >= time_[x.b] ? x.a : x.b;
                    if (va != UNKNOWN && vb != UNKNOWN) v = static_cast<int8_t>(1 - controlling);
                }
                if (inverted && v != UNKNOWN) v = static_cast<int8_t>(1 - v);
                s = time_[p];
                break;
            }
            case Kind::Xor: {
                const int8_t va = value_[x.a], vb = value_[x.b];
                p = time_[x.a] >= time_[x.b] ? x.a : x.b;
                if (va != UNKNOWN && vb != UNKNOWN) v = static_cast<int8_t>(va ^ vb);
                s = time_[p];
                break;
            }
            case Kind::Hub: {
                const auto& hub = hubs_[x.hub];
                s = hub.launch;
                for (const auto& [in, d] : hub.inputs)
                    if (time_[in] != STABLE && time_[in] + d > s) { s = time_[in] + d; p = in; }
                break;
            }
            }
            if (x.kind != Kind::Source && x.kind != Kind::Hub && s != STABLE) s += x.delay;
            if (s == STABLE) p = -1;
            if (!values) v = UNKNOWN;
            else if (const int8_t assumed = forced_[n]; assumed != UNKNOWN) {
                if (v == UNKNOWN) {
                    v = assumed;
                    // A case variable is free; any other assumed bit still has
                    // to be produced by the sources below it.
                    if (x.var < 0) unjustified_.push_back(n);
                } else if (v != assumed) conflict_ = true;
            }
            value_[n] = v;
            time_[n] = s;
            previous_[n] = p;
        }
    }

    std::pair<long long, int> latest(const Group& group) const {
        long long best = STABLE;
        int where = -1;
        for (const auto& [n, extra] : group.endpoints)
            if (time_[n] != STABLE && time_[n] + extra > best) { best = time_[n] + extra; where = n; }
        return {best, where};
    }

    // Assume a final value, with everything that follows from it at once:
    // AND = 1 needs both inputs 1, OR = 0 needs both 0. False: contradiction.
    bool force(int start, int8_t start_value) {
        std::vector<std::pair<int, int8_t>> pending{{start, start_value}};
        while (!pending.empty()) {
            const auto [n, v] = pending.back();
            pending.pop_back();
            if (forced_[n] == v) continue;
            const auto& x = nodes_[n];
            if (forced_[n] != UNKNOWN || (x.constant != UNKNOWN && x.constant != v)) return false;
            forced_[n] = v;
            touched_.push_back(n);
            const auto both = [&](int8_t value) { pending.emplace_back(x.a, value); pending.emplace_back(x.b, value); };
            switch (x.kind) {
            case Kind::Buf: pending.emplace_back(x.a, v); break;
            case Kind::Not: pending.emplace_back(x.a, static_cast<int8_t>(1 - v)); break;
            case Kind::And: if (v == 1) both(1); break;
            case Kind::Nand: if (v == 0) both(1); break;
            case Kind::Or: if (v == 0) both(0); break;
            case Kind::Nor: if (v == 1) both(0); break;
            default: break;
            }
        }
        return true;
    }
    // Consequences that need a known input: AND = 0 with one input 1 makes the
    // other 0, and so on. -1 contradiction, 0 nothing new, 1 something new.
    int imply() {
        bool added = false;
        bool consistent = true;
        const auto need = [&](int in, int8_t value) {
            if (value_[in] != UNKNOWN) { consistent &= value_[in] == value; return; }
            const size_t before = touched_.size();
            consistent &= force(in, value);
            added |= touched_.size() > before;
        };
        for (size_t i = 0; i < touched_.size() && consistent; ++i) {
            const int n = touched_[i];
            const auto& x = nodes_[n];
            const int8_t v = forced_[n];
            switch (x.kind) {
            case Kind::And: case Kind::Nand: case Kind::Or: case Kind::Nor: {
                const int8_t controlling = x.kind == Kind::And || x.kind == Kind::Nand ? 0 : 1;
                const bool inverted = x.kind == Kind::Nand || x.kind == Kind::Nor;
                // Output at its controlled value: some input is controlling.
                if ((inverted ? 1 - v : v) != controlling) break;
                const int8_t other = static_cast<int8_t>(1 - controlling);
                if (value_[x.a] == other) need(x.b, controlling);
                else if (value_[x.b] == other) need(x.a, controlling);
                break;
            }
            case Kind::Xor:
                if (value_[x.a] != UNKNOWN) need(x.b, static_cast<int8_t>(v ^ value_[x.a]));
                else if (value_[x.b] != UNKNOWN) need(x.a, static_cast<int8_t>(v ^ value_[x.b]));
                break;
            default: break;
            }
        }
        return !consistent ? -1 : added ? 1 : 0;
    }
    void clearCase() {
        for (const int n : touched_) forced_[n] = UNKNOWN;
        touched_.clear();
    }
    // False: no final vector satisfies the case.
    bool evaluate(const std::vector<int>& order) {
        for (int pass = 0;; ++pass) {
            run(order, true);
            if (conflict_) return false;
            // Each pass costs one evaluation and only tightens the case, so
            // stopping early is safe; long carry chains need many passes.
            if (pass == 256) return true;
            const int more = imply();
            if (more < 0) return false;
            if (more == 0) return true;
        }
    }

    // An undecided case variable at or below `start`. `open_only` follows
    // only bits whose value is still open: the variables that can decide it.
    int openVariable(int start, bool open_only) {
        ++stamp_counter_;
        std::vector<int> pending{start};
        while (!pending.empty()) {
            const int n = pending.back();
            pending.pop_back();
            if (stamp_[n] == stamp_counter_ || (open_only && value_[n] != UNKNOWN)) continue;
            stamp_[n] = stamp_counter_;
            if (nodes_[n].var >= 0 && forced_[n] == UNKNOWN) return n;
            inputsOf(n, [&](int in) { pending.push_back(in); });
        }
        return -1;
    }
    // What to decide next on the limiting path: a side input whose value is
    // still open. Deciding it either removes the path or keeps it. With
    // `internal` the side input itself is decided; otherwise a case variable
    // below it. `opaque` reports open side inputs that may be X or Z.
    int openSide(int critical, bool internal, bool& opaque) {
        for (int n = critical; n >= 0; n = previous_[n]) {
            const auto& node = nodes_[n];
            if (node.kind != Kind::And && node.kind != Kind::Nand && node.kind != Kind::Or && node.kind != Kind::Nor)
                continue;
            const int side = previous_[n] == node.a ? node.b : node.a;
            if (value_[side] != UNKNOWN) continue;
            if (!binary_[side]) { opaque = true; continue; }
            if (internal) return side;
            if (const int variable = openVariable(side, true); variable >= 0) return variable;
        }
        return -1;
    }
    // The case variable that the most assumed-but-not-yet-produced bits
    // depend on (counted along open paths). Deciding a shared one first, such
    // as a carry-in or an operation bit, lets one decision settle many
    // assumptions instead of enumerating operand values that do not matter.
    int openJustification(const std::vector<int>& order) {
        if (unjustified_.empty()) return -1;
        for (const int n : order) weight_[n] = 0;
        for (const int n : unjustified_) weight_[n] = 1;
        int best = -1;
        for (size_t i = order.size(); i-- > 0;) {
            const int n = order[i];
            const double w = weight_[n];
            if (w == 0) continue;
            if (nodes_[n].var >= 0 && forced_[n] == UNKNOWN && (best < 0 || w > weight_[best])) best = n;
            inputsOf(n, [&](int in) { if (value_[in] == UNKNOWN) weight_[in] += w; });
        }
        return best;
    }

    SettlingEndpoint solve(const Group& group, bool internal, size_t budget) {
        SettlingEndpoint result;
        result.name = group.name;
        std::vector<int> order, variables;
        {
            ++stamp_counter_;
            std::vector<int> pending;
            for (const auto& [n, extra] : group.endpoints) pending.push_back(n);
            while (!pending.empty()) {
                const int n = pending.back();
                pending.pop_back();
                if (stamp_[n] == stamp_counter_) continue;
                stamp_[n] = stamp_counter_;
                if (nodes_[n].var >= 0) variables.push_back(nodes_[n].var);
                inputsOf(n, [&](int in) { pending.push_back(in); });
            }
            for (const int n : order_) if (stamp_[n] == stamp_counter_) order.push_back(n);
        }
        const size_t start = evaluations_;
        clearCase();
        run(order, false);
        const auto structural = latest(group).first;
        if (structural == STABLE) return result;
        result.changes = true;
        result.structural = static_cast<size_t>(structural);

        struct Choice { std::shared_ptr<const Choice> parent; int node; int8_t value; };
        struct Open { long long ceiling; int depth; std::shared_ptr<const Choice> choice; };
        const auto later = [](const Open& a, const Open& b) {
            return a.ceiling != b.ceiling ? a.ceiling < b.ceiling : a.depth < b.depth;
        };
        std::priority_queue<Open, std::vector<Open>, decltype(later)> open(later);
        const auto load = [&](const std::shared_ptr<const Choice>& choice) {
            clearCase();
            for (auto c = choice; c; c = c->parent) if (!force(c->node, c->value)) return false;
            return evaluate(order);
        };
        // A complete final vector, evaluated with nothing assumed: the value it
        // gives is reached by a real case, so no bound below it exists.
        const auto plain = [&](const std::vector<int8_t>& vector) {
            clearCase();
            for (const int var : variables) force(variable_node_[var], vector[var]);
            run(order, true);
            return latest(group).first;
        };
        long long attained = STABLE - 1;
        std::vector<int8_t> attaining(variables_.size(), UNKNOWN);
        std::mt19937_64 random(options_.seed);
        if (!variables.empty()) for (int mode = 0; mode < 8; ++mode) {
            std::vector<int8_t> vector(variables_.size(), UNKNOWN);
            for (const int var : variables) vector[var] = static_cast<int8_t>(mode < 2 ? mode : random() & 1);
            if (const auto reached = plain(vector); reached > attained) { attained = reached; attaining = std::move(vector); }
        }

        long long bound = structural;
        std::shared_ptr<const Choice> limiting;
        bool from_vector = false;
        if (load(nullptr)) open.push({latest(group).first, 0, nullptr});
        const long long unrestricted = open.empty() ? structural : open.top().ceiling;
        while (true) {
            // Cases only disappear by contradiction, and some real vector is
            // always left; should none remain, claim nothing beyond the start.
            if (open.empty()) { bound = unrestricted; limiting = nullptr; break; }
            const auto top = open.top();
            bound = top.ceiling;
            limiting = top.choice;
            if (top.ceiling <= attained) { bound = attained; from_vector = true; break; }
            if (evaluations_ - start >= budget) break;
            open.pop();
            if (!load(top.choice)) continue;
            const int critical = latest(group).second;
            bool opaque = false;
            int branch = critical < 0 ? -1 : openSide(critical, internal, opaque);
            if (branch < 0) branch = openJustification(order);
            // A decided path is not enough: a side input that also controls
            // may still settle earlier. Decide everything below the endpoint.
            if (branch < 0 && critical >= 0) branch = openVariable(critical, false);
            if (branch < 0) {
                // One complete final vector below the limiting endpoint.
                // Confirm its value with nothing assumed.
                std::vector<int8_t> vector(variables_.size(), UNKNOWN);
                for (const int var : variables) {
                    const int8_t decided = forced_[variable_node_[var]];
                    vector[var] = decided == UNKNOWN ? int8_t{0} : decided;
                }
                const auto reached = plain(vector);
                if (reached > attained) { attained = reached; attaining = std::move(vector); }
                from_vector = reached == top.ceiling;
                break;
            }
            for (int8_t value = 0; value < 2; ++value) {
                const auto child = std::make_shared<const Choice>(Choice{top.choice, branch, value});
                if (load(child)) open.push({latest(group).first, top.depth + 1, child});
            }
        }

        result.evaluations = evaluations_ - start;
        bool sensitized = false;
        if (from_vector) {
            plain(attaining);
            bool opaque = false;
            sensitized = latest(group).second < 0 || (openSide(latest(group).second, true, opaque) < 0 && !opaque);
        } else if (!load(limiting)) {
            clearCase();
            run(order, true);
        }
        const int critical = latest(group).second;
        if (bound == STABLE) { result.changes = false; result.structural = 0; clearCase(); return result; }
        result.bound = static_cast<size_t>(bound);
        for (int n = critical; n >= 0; n = previous_[n]) result.path.push_back(label(n));
        std::reverse(result.path.begin(), result.path.end());
        if (from_vector) {
            result.tight = sensitized;
            for (const int var : variables) if (variables_[var].second < 64)
                result.witness[variables_[var].first->getID()] |= uint64_t(attaining[var]) << variables_[var].second;
        }
        clearCase();
        return result;
    }

public:
    Analysis(const std::shared_ptr<Component>& root, const SettlingOptions& options)
        : root_(root), options_(options) {}

    SettlingReport analyze() {
        try {
            root_io_ = std::dynamic_pointer_cast<IOComponent>(root_);
            if (!root_io_) throw std::runtime_error("Root has no pins");
            collect(root_);
            if (failed_) return report_;

            for (const auto* pins : {&root_io_->getAllInputPins(), &root_io_->getAllOutputPins()})
                for (const auto& [name, pin] : *pins) reserve(pin);
            for (const auto& leaf : leaves_)
                for (const auto* pins : {&leaf.component->getAllInputPins(), &leaf.component->getAllOutputPins()})
                    for (const auto& [name, pin] : *pins) reserve(pin);

            if (!root_is_leaf_)
                for (const auto& [name, pin] : root_io_->getAllInputPins()) trace(pin, pin->getInternalWireBase());
            for (const auto& leaf : leaves_) for (const auto& [name, pin] : leaf.component->getAllOutputPins()) {
                trace(pin, pin->getExternalWireBase());
                // A leaf reading its own output is wired on the internal side.
                if (std::dynamic_pointer_cast<BasicComponent>(leaf.component)) trace(pin, pin->getInternalWireBase());
            }

            // One ideal clock, taken straight from a root input.
            for (const auto& leaf : leaves_) {
                if (leaf.contract.clock_pin.empty()) {
                    if (!leaf.contract.captures.empty() || !leaf.contract.launches.empty())
                        fail(leaf.component->getID() + ": clocked contract lacks a clock pin");
                    continue;
                }
                const auto pin = leaf.component->getInputPinDynamic(leaf.contract.clock_pin);
                if (!pin) throw std::runtime_error(leaf.component->getID() + ": contract names a missing clock pin");
                clock_sinks_.insert(pin.get());
                if (leaf.component == root_) { clock_roots_.insert(pin.get()); continue; }
                const auto source = driver_.find(pin.get());
                if (source == driver_.end() || !rootPin(*source->second))
                    fail(pin->getID() + ": derived, gated or undriven clock is unsupported");
                else clock_roots_.insert(source->second.get());
            }
            for (const auto& [sink, source] : driver_)
                if (clock_roots_.contains(source.get()) && !clock_sinks_.contains(sink))
                    fail(source->getID() + ": clock used as data");
            for (const auto& [name, value] : options_.fixed_inputs)
                if (!root_io_->getInputPinDynamic(name)) fail("fixed_inputs names no root input: " + name);
            for (const auto& [name, value] : options_.final_inputs)
                if (!root_io_->getInputPinDynamic(name) || options_.fixed_inputs.contains(name))
                    fail("final_inputs must name a root input that is not fixed: " + name);
            for (const auto& [name, tick] : options_.input_arrival)
                if (!root_io_->getInputPinDynamic(name)) fail("input_arrival names no root input: " + name);
            if (failed_) return report_;

            groups_.push_back({"(captures)", {}});
            if (!root_is_leaf_) {
                for (const auto& [name, pin] : root_io_->getAllInputPins()) makeRootInput(pin);
                for (const auto& [name, pin] : root_io_->getAllOutputPins()) {
                    const auto source = driver_.find(pin.get());
                    if (source == driver_.end()) continue;
                    for (unsigned bit = 0; bit < pin->getWidth(); ++bit) {
                        auto& node = at(pin, bit);
                        node.kind = Kind::Buf;
                        node.a = index(source->second, bit);
                    }
                }
            }
            for (const auto& leaf : leaves_) makeLeaf(leaf);
            for (const auto& [name, pin] : root_io_->getAllOutputPins()) {
                Group group{name, {}};
                for (unsigned bit = 0; bit < pin->getWidth(); ++bit) group.endpoints.emplace_back(index(pin, bit), 0);
                groups_.push_back(std::move(group));
            }
            if (failed_ || !sort()) return report_;

            value_.assign(nodes_.size(), UNKNOWN);
            time_.assign(nodes_.size(), STABLE);
            previous_.assign(nodes_.size(), -1);
            stamp_.assign(nodes_.size(), 0);
            forced_.assign(nodes_.size(), UNKNOWN);
            weight_.assign(nodes_.size(), 0);
            binary_.assign(nodes_.size(), 0);
            for (const int n : order_) {
                const auto& x = nodes_[n];
                switch (x.kind) {
                case Kind::Stable: binary_[n] = x.constant != UNKNOWN; break;
                case Kind::Source: binary_[n] = x.var >= 0 || x.constant != UNKNOWN; break;
                case Kind::Buf: case Kind::Not: binary_[n] = binary_[x.a]; break;
                case Kind::Hub: break;
                default: binary_[n] = binary_[x.a] && binary_[x.b]; break;
                }
            }
            run(order_, true);
            for (const auto& [n, value, id] : required_)
                if (time_[n] != STABLE || value_[n] != value)
                    fail(id + ": contract requires the fixed value " + std::to_string(value)
                         + "; state it with fixed_inputs");
            if (failed_) return report_;

            for (const auto& group : groups_) {
                if (group.endpoints.empty()) continue;
                // Two ways of choosing cases; both are sound, so the lower
                // result stands. The second runs only if the first is open.
                const bool both = options_.search == SettlingSearch::Both;
                auto result = solve(group, options_.search == SettlingSearch::Signals,
                                    both ? options_.search_budget / 2 : options_.search_budget);
                if (both && result.changes && !result.tight) {
                    auto other = solve(group, true, options_.search_budget - result.evaluations);
                    other.evaluations += result.evaluations;
                    if (!other.changes || other.bound < result.bound || (other.bound == result.bound && other.tight))
                        result = std::move(other);
                    else result.evaluations = other.evaluations;
                }
                report_.endpoints.push_back(std::move(result));
                report_.structural = std::max(report_.structural, report_.endpoints.back().structural);
                report_.bound = std::max(report_.bound, report_.endpoints.back().bound);
            }
            report_.bit_count = nodes_.size();
            report_.variable_count = variables_.size();
            report_.premises = {
                "Every evaluated part publishes f(inputs at t) at exactly t + its delay",
                "The supplied and built-in component contracts are sound",
                "Start-up has passed: logic fed only by unchanging sources is already quiet",
                "One ideal clock taken directly from a root input; rising-edge state only",
                options_.binary_sources
                    ? "ENVIRONMENT: final root-input and clocked-state values are 0 or 1 (earlier values unrestricted)"
                    : "No value restriction: final source values may be 0, 1, X or Z",
                options_.sources == SettlingSources::All ? "Root inputs change at their arrival tick and state launches at the clock edge"
                    : options_.sources == SettlingSources::InputsOnly ? "Only root inputs change; clocked state holds"
                    : "Only clocked state launches; root inputs hold"};
            report_.valid = true;
        } catch (const std::exception& error) {
            fail(error.what());
            report_.valid = false;
            report_.endpoints.clear();
        }
        return report_;
    }
};

} // namespace

SettlingReport analyzeSettling(const std::shared_ptr<Component>& root, const SettlingOptions& options) {
    if (!root) {
        SettlingReport report;
        report.diagnostics.push_back("No circuit");
        return report;
    }
    return Analysis(root, options).analyze();
}

} // namespace circuit::analysis
