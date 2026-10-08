#include "TimingEngine.hpp"
#include "TimedLogicDomain.hpp"
#include <algorithm>
#include <array>
#include <memory>
#include <queue>

namespace circuit::analysis::detail::timing {
namespace {

bool single(uint8_t values) { return values && (values & (values - 1)) == 0; }
Tick add(Tick time, Tick delay) { return time == OPEN || time == NEVER ? time : time + delay; }

// Four-state gate result over the codes 0 = LOW, 1 = HIGH, 2 = X, 3 = Z,
// exactly as the gates evaluate.
unsigned gateValue(Kind kind, unsigned a, unsigned b) {
    unsigned v = 2;
    if (kind == Kind::And || kind == Kind::Nand) v = !a || !b ? 0 : a == 1 && b == 1 ? 1 : 2;
    else if (kind == Kind::Or || kind == Kind::Nor) v = a == 1 || b == 1 ? 1 : !a && !b ? 0 : 2;
    else if (kind == Kind::Xor) v = a < 2 && b < 2 ? (a != b) : 2;
    return (kind == Kind::Nand || kind == Kind::Nor) && v < 2 ? 1 - v : v;
}
// image[gate][set of A][set of B] = set of possible outputs.
struct Images {
    std::array<std::array<std::array<uint8_t, 16>, 16>, 5> gate{};
    std::array<uint8_t, 16> inverted{};
    Images() {
        const Kind kinds[] = {Kind::And, Kind::Nand, Kind::Or, Kind::Nor, Kind::Xor};
        for (size_t k = 0; k < 5; ++k) for (unsigned a = 0; a < 16; ++a) for (unsigned b = 0; b < 16; ++b)
            for (unsigned x = 0; x < 4; ++x) for (unsigned y = 0; y < 4; ++y)
                if ((a & (1u << x)) && (b & (1u << y))) gate[k][a][b] |= uint8_t(1u << gateValue(kinds[k], x, y));
        for (unsigned a = 0; a < 16; ++a) for (unsigned x = 0; x < 4; ++x)
            if (a & (1u << x)) inverted[a] |= uint8_t(x == 0 ? HIGH : x == 1 ? LOW : X);
    }
};
const Images IMAGES;
size_t gateIndex(Kind kind) { return static_cast<size_t>(kind) - static_cast<size_t>(Kind::And); }
bool controllable(Kind kind) { return kind == Kind::And || kind == Kind::Nand || kind == Kind::Or || kind == Kind::Nor; }

} // namespace

Engine::Engine(const Graph& graph, uint64_t seed)
    : graph_(graph), random_(seed), fact_(graph.nodes.size()), raw_(graph.nodes.size(), ANY),
      known_(graph.nodes.size(), ANY), previous_(graph.nodes.size(), -1), stamp_(graph.nodes.size(), 0),
      cone_(graph.nodes.size(), 0), queued_(graph.nodes.size(), 0), forced_(graph.nodes.size(), ANY),
      weight_(graph.nodes.size(), 0) {}

int Engine::value(int n) const {
    const uint8_t values = fact_[n].values & forced_[n];
    return values == LOW ? 0 : values == HIGH ? 1 : 2;
}

// Tighten the fact of bit `n` with one more thing that is known about it.
void Engine::apply(int n, uint8_t values, Tick at, Tick settled, int previous) {
    auto& f = fact_[n];
    raw_[n] &= values;
    const uint8_t narrowed = f.values & values;
    if (!narrowed) conflict_ = true;
    // Two facts together hold from the later of their starting ticks.
    else if (narrowed != f.values) { f.values = narrowed; f.at = std::max(f.at, at); }
    else if (values == f.values && at < f.at) f.at = at;
    if (settled < f.settled) { f.settled = settled; previous_[n] = previous; }
    // Only one possible value: nothing can change once that is established.
    if (single(f.values) && f.at < f.settled) f.settled = f.at;
}

void Engine::refineGate(int n, const Node& x, bool values) {
    const Fact& a = fact_[x.a];
    const Fact& b = fact_[x.b];
    // Settled one delay after both inputs are.
    const int latest = a.settled >= b.settled ? x.a : x.b;
    const Tick settled = add(std::max(a.settled, b.settled), x.delay);
    if (!values) { apply(n, ANY, NEVER, settled, latest); return; }
    const auto& image = IMAGES.gate[gateIndex(x.kind)];
    const uint8_t out = image[a.values][b.values];
    // The output set holds once both input sets do, or as soon as one input
    // alone already gives it (a controlling value).
    Tick at = std::max(a.at, b.at);
    int decider = a.at >= b.at ? x.a : x.b;
    if (image[a.values][ANY] == out && a.at < at) { at = a.at; decider = x.a; }
    if (image[ANY][b.values] == out && b.at < at) { at = b.at; decider = x.b; }
    at = add(at, x.delay);
    if (single(out) && at < settled) apply(n, out, at, at, decider);
    else apply(n, out, at, settled, latest);
}

// The compact FF endpoint. Empty: REQ_READY is 1 and the response follows
// the request combinationally. Holding a response: RSP_VALID is 1, the saved
// response is published one delay after the edge, and only REQ_READY follows
// RSP_READY. Without a mode, every arc of either mode is kept.
void Engine::refinePart(int n, const Node& x, bool values) {
    const Part& part = graph_.parts[x.owner];
    const uint8_t mode = case_ && static_cast<size_t>(x.owner) < case_->part_mode.size()
        ? case_->part_mode[x.owner] : uint8_t(AnyMode);
    const Tick d = part.delay;
    // Latest of the launch and the listed input bits, one delay on.
    const auto arcs = [&](std::initializer_list<std::pair<int, unsigned>> ranges) {
        Tick settled = d;
        int latest = -1;
        for (const auto& [first, count] : ranges) for (unsigned i = 0; i < count; ++i) {
            const Tick t = fact_[first + int(i)].settled;
            if (t == OPEN) { apply(n, ANY, NEVER, OPEN, -1); return; }
            if (t != NEVER && t + d > settled) { settled = t + d; latest = first + int(i); }
        }
        apply(n, ANY, NEVER, settled, latest);
    };
    // HIGH exactly when the input is HIGH; anything else gives LOW.
    const auto follows = [&](int input) {
        const Fact& in = fact_[input];
        const uint8_t out = uint8_t((in.values & HIGH ? HIGH : 0) | (in.values & ~HIGH ? LOW : 0));
        apply(n, out, add(in.at, d), OPEN, input);
    };
    if (values && case_ && case_->binary_parts) apply(n, BINARY, d, OPEN, -1);
    switch (x.output) {
    case Ready:
        if (mode == Empty) { apply(n, values ? HIGH : ANY, d, d, -1); break; }
        arcs({{part.rsp_ready, 1}});
        if (mode == Full && values) follows(part.rsp_ready);
        break;
    case Valid:
        if (mode == Full) { apply(n, values ? HIGH : ANY, d, d, -1); break; }
        arcs({{part.req_valid, 1}});
        if (mode == Empty && values) follows(part.req_valid);
        break;
    case Data:
        if (mode == Full) { apply(n, ANY, NEVER, d, -1); break; }
        arcs({{part.req_valid, 1}, {part.op, 3}, {part.address, part.address_bits}});
        // No request can be valid: the payload is zero.
        if (mode == Empty && values && !(fact_[part.req_valid].values & HIGH)) {
            const Tick t = add(fact_[part.req_valid].at, d);
            apply(n, LOW, t, t, part.req_valid);
        }
        break;
    case Status: {
        if (mode == Full) { apply(n, ANY, NEVER, d, -1); break; }
        arcs({{part.op, 3}, {part.address, part.address_bits}, {part.mask, part.mask_bits}});
        if (mode != Empty || !values) break;
        // The request validator, over the sets its inputs can take.
        const auto bits = [&](int first, unsigned count) {
            std::vector<TimedBit> result(count);
            for (unsigned i = 0; i < count; ++i) {
                const Fact& f = fact_[first + int(i)];
                result[i] = {f.values, f.at == NEVER ? size_t{0} : static_cast<size_t>(f.at),
                             f.settled == OPEN ? unbounded : f.settled == NEVER ? size_t{0} : static_cast<size_t>(f.settled)};
            }
            return result;
        };
        const auto proof = ffStatusBounds(bits(part.op, 3), bits(part.address, part.address_bits),
            bits(part.mask, part.mask_bits), part.bytes_per_word, part.capacity_bytes, static_cast<size_t>(d));
        const auto& p = proof[graph_.nodes[n].bit];
        apply(n, static_cast<uint8_t>(p.values), static_cast<Tick>(p.at),
              p.settled == unbounded ? OPEN : static_cast<Tick>(p.settled), -1);
        break;
    }
    }
}

// Recompute bit `n` from its inputs. True: its fact tightened.
bool Engine::refine(int n, bool values) {
    const Node& x = graph_.nodes[n];
    const Fact before = fact_[n];
    switch (x.kind) {
    case Kind::Stable:
        apply(n, values && x.constant < 2 ? (x.constant ? HIGH : LOW) : ANY, NEVER, NEVER, -1);
        break;
    case Kind::Source: {
        uint8_t set = ANY;
        if (values) set = uint8_t((x.constant < 2 ? (x.constant ? HIGH : LOW) : x.var >= 0 ? BINARY : ANY) & known_[n]);
        apply(n, set, x.delay, x.delay, -1);
        break;
    }
    case Kind::Buf: case Kind::Not: {
        const Fact& in = fact_[x.a];
        const uint8_t set = !values ? ANY : x.kind == Kind::Not ? IMAGES.inverted[in.values] : in.values;
        apply(n, set, add(in.at, x.delay), add(in.settled, x.delay), x.a);
        break;
    }
    case Kind::Hub: {
        const Hub& hub = graph_.hubs[x.owner];
        Tick settled = hub.launch, at = hub.launch;
        int latest = -1;
        bool open = false, binary = hub.binary_with_inputs && values && case_ && case_->binary_parts;
        for (const auto& [in, d] : hub.inputs) {
            const Fact& f = fact_[in];
            if (f.settled == OPEN) open = true;
            else if (f.settled != NEVER && f.settled + d > settled) { settled = f.settled + d; latest = in; }
            if (f.values & ~BINARY) binary = false;
            else at = std::max(at, add(f.at, d));
        }
        apply(n, binary ? BINARY : ANY, binary ? at : NEVER, open ? OPEN : settled, latest);
        break;
    }
    case Kind::Part: refinePart(n, x, values); break;
    default: refineGate(n, x, values); break;
    }
    // An assumed final value holds from the tick the bit settles.
    if (values && forced_[n] != ANY) {
        auto& f = fact_[n];
        if (!(raw_[n] & forced_[n])) conflict_ = true;
        else if (f.settled != OPEN && (f.values & ~forced_[n])) {
            f.values &= forced_[n];
            f.at = std::max(f.at, f.settled);
            if (single(f.values) && f.at < f.settled) f.settled = f.at;
        }
    }
    return !(fact_[n] == before);
}

// Tighten until nothing moves. `values == false` ignores every value (the
// per-bit longest path).
void Engine::run(const std::vector<int>& order, bool values) {
    ++evaluations_;
    ++cone_counter_;
    conflict_ = false;
    for (const int n : order) {
        fact_[n] = Fact{};
        raw_[n] = ANY;
        previous_[n] = -1;
        cone_[n] = cone_counter_;
        queued_[n] = 1;
    }
    queue_.assign(order.begin(), order.end());
    for (size_t head = 0; head < queue_.size(); ++head) {
        const int n = queue_[head];
        queued_[n] = 0;
        ++work_;
        if (!refine(n, values)) continue;
        for (const int reader : graph_.readers[n])
            if (cone_[reader] == cone_counter_ && !queued_[reader]) { queued_[reader] = 1; queue_.push_back(reader); }
    }
    // A case variable is free; any other assumed bit still has to be
    // produced by the sources below it.
    unjustified_.clear();
    if (values) for (const int n : touched_)
        if (cone_[n] == cone_counter_ && graph_.nodes[n].var < 0 && (raw_[n] & ~forced_[n])) unjustified_.push_back(n);
}

void Engine::evaluateAll() {
    clearAssumptions();
    std::fill(known_.begin(), known_.end(), ANY);
    if (case_) for (const auto& [n, values] : case_->known) known_[n] &= values;
    run(graph_.order, true);
}

std::vector<int> Engine::pathTo(int n) const {
    std::vector<int> path;
    for (; n >= 0 && path.size() <= graph_.nodes.size(); n = previous_[n]) path.push_back(n);
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<Tick> Engine::earliest() const {
    std::vector<Tick> first(graph_.nodes.size(), OPEN);
    using Entry = std::pair<Tick, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> heap;
    const auto reach = [&](int n, Tick tick) {
        if (tick < first[n]) { first[n] = tick; heap.emplace(tick, n); }
    };
    for (int n = 0; n < static_cast<int>(graph_.nodes.size()); ++n) {
        const auto& x = graph_.nodes[n];
        if (x.kind == Kind::Source && x.delay != NEVER) reach(n, x.earliest != OPEN ? x.earliest : x.delay);
        else if (x.kind == Kind::Hub && graph_.hubs[x.owner].launch_minimum != OPEN) reach(n, graph_.hubs[x.owner].launch_minimum);
        else if (x.kind == Kind::Part) reach(n, graph_.parts[x.owner].delay);
    }
    while (!heap.empty()) {
        const auto [tick, n] = heap.top();
        heap.pop();
        if (tick != first[n]) continue;
        for (const int reader : graph_.readers[n]) {
            const auto& x = graph_.nodes[reader];
            Tick delay = x.delay;
            if (x.kind == Kind::Part) delay = graph_.parts[x.owner].delay;
            else if (x.kind == Kind::Hub) {
                const auto& hub = graph_.hubs[x.owner];
                delay = OPEN;
                for (size_t i = 0; i < hub.inputs.size(); ++i)
                    if (hub.inputs[i].first == n) delay = std::min(delay, hub.minimum[i]);
            }
            if (delay != OPEN) reach(reader, tick + delay);
        }
    }
    return first;
}

std::pair<Tick, int> Engine::latest(const Group& group) const {
    Tick best = NEVER;
    int where = -1;
    for (const auto& [n, extra] : group.endpoints) {
        const Tick t = fact_[n].settled;
        if (t == OPEN) return {OPEN, n};
        if (t != NEVER && t + extra > best) { best = t + extra; where = n; }
    }
    return {best, where};
}

// Assume a final value, with everything that follows from it at once:
// AND = 1 needs both inputs 1, OR = 0 needs both 0. False: contradiction.
bool Engine::force(int start, uint8_t start_value) {
    std::vector<std::pair<int, uint8_t>> pending{{start, start_value}};
    while (!pending.empty()) {
        const auto [n, v] = pending.back();
        pending.pop_back();
        if (forced_[n] == v) continue;
        const auto& x = graph_.nodes[n];
        if (forced_[n] != ANY || (x.constant < 2 && (x.constant ? HIGH : LOW) != v)) return false;
        forced_[n] = v;
        touched_.push_back(n);
        const uint8_t opposite = v == LOW ? HIGH : LOW;
        const auto both = [&](uint8_t value) { pending.emplace_back(x.a, value); pending.emplace_back(x.b, value); };
        switch (x.kind) {
        case Kind::Buf: pending.emplace_back(x.a, v); break;
        case Kind::Not: pending.emplace_back(x.a, opposite); break;
        case Kind::And: if (v == HIGH) both(HIGH); break;
        case Kind::Nand: if (v == LOW) both(HIGH); break;
        case Kind::Or: if (v == LOW) both(LOW); break;
        case Kind::Nor: if (v == HIGH) both(LOW); break;
        default: break;
        }
    }
    return true;
}

// Consequences that need a known input: AND = 0 with one input 1 makes the
// other 0, and so on. -1 contradiction, 0 nothing new, 1 something new.
int Engine::imply() {
    bool added = false, consistent = true;
    const auto need = [&](int in, int wanted) {
        if (value(in) != 2) { consistent &= value(in) == wanted; return; }
        const size_t before = touched_.size();
        consistent &= force(in, wanted ? HIGH : LOW);
        added |= touched_.size() > before;
    };
    for (size_t i = 0; i < touched_.size() && consistent; ++i) {
        const int n = touched_[i];
        const auto& x = graph_.nodes[n];
        const int v = forced_[n] == HIGH ? 1 : 0;
        if (controllable(x.kind)) {
            const int controlling = x.kind == Kind::And || x.kind == Kind::Nand ? 0 : 1;
            const bool inverted = x.kind == Kind::Nand || x.kind == Kind::Nor;
            // Output at its controlled value: some input is controlling.
            if ((inverted ? 1 - v : v) != controlling) continue;
            if (value(x.a) == 1 - controlling) need(x.b, controlling);
            else if (value(x.b) == 1 - controlling) need(x.a, controlling);
        } else if (x.kind == Kind::Xor) {
            if (value(x.a) != 2) need(x.b, v ^ value(x.a));
            else if (value(x.b) != 2) need(x.a, v ^ value(x.b));
        }
    }
    return !consistent ? -1 : added ? 1 : 0;
}

void Engine::clearAssumptions() {
    for (const int n : touched_) forced_[n] = ANY;
    touched_.clear();
}

// False: no final vector satisfies the case.
bool Engine::evaluate(const std::vector<int>& order) {
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

// An undecided case variable at or below `start`. `open_only` follows only
// bits whose value is still open: the variables that can decide it.
int Engine::openVariable(int start, bool open_only) {
    ++stamp_counter_;
    std::vector<int> pending{start};
    while (!pending.empty()) {
        const int n = pending.back();
        pending.pop_back();
        if (stamp_[n] == stamp_counter_ || (open_only && value(n) != 2)) continue;
        stamp_[n] = stamp_counter_;
        if (graph_.nodes[n].var >= 0 && forced_[n] == ANY) return n;
        graph_.inputsOf(n, [&](int in) { pending.push_back(in); });
    }
    return -1;
}

// What to decide next on the limiting path: a side input whose value is
// still open. Deciding it either removes the path or keeps it. With
// `signals` the side input itself is decided; otherwise a case variable
// below it. `opaque` reports open side inputs that may be X or Z.
int Engine::openSide(int critical, bool signals, bool& opaque) {
    for (int n = critical; n >= 0; n = previous_[n]) {
        const auto& node = graph_.nodes[n];
        if (!controllable(node.kind)) continue;
        const int side = previous_[n] == node.a ? node.b : node.a;
        if (value(side) != 2) continue;
        if ((fact_[side].values & ~BINARY) || fact_[side].settled == OPEN) { opaque = true; continue; }
        if (signals) return side;
        if (const int variable = openVariable(side, true); variable >= 0) return variable;
    }
    return -1;
}

// The case variable that the most assumed-but-not-yet-produced bits depend
// on (counted along open paths). Deciding a shared one first, such as a
// carry-in or an operation bit, lets one decision settle many assumptions
// instead of enumerating operand values that do not matter.
int Engine::openJustification(const std::vector<int>& order) {
    if (unjustified_.empty()) return -1;
    for (const int n : order) weight_[n] = 0;
    for (const int n : unjustified_) weight_[n] = 1;
    int best = -1;
    for (size_t i = order.size(); i-- > 0;) {
        const int n = order[i];
        const double w = weight_[n];
        if (w == 0) continue;
        if (graph_.nodes[n].var >= 0 && forced_[n] == ANY && (best < 0 || w > weight_[best])) best = n;
        graph_.inputsOf(n, [&](int in) { if (value(in) == 2) weight_[in] += w; });
    }
    return best;
}

// A loop stays open until a value on it is decided. Below an unsettled bit,
// find a gate with an input that is settled and is 0 or 1 but not yet
// decided: either value of it may close the loop.
int Engine::openLoop(int unsettled) {
    ++stamp_counter_;
    std::vector<int> pending{unsettled};
    while (!pending.empty()) {
        const int n = pending.back();
        pending.pop_back();
        if (stamp_[n] == stamp_counter_ || fact_[n].settled != OPEN) continue;
        stamp_[n] = stamp_counter_;
        const auto& node = graph_.nodes[n];
        if (controllable(node.kind)) for (const int in : {node.a, node.b}) {
            const Fact& f = fact_[in];
            if (f.settled != OPEN && !(f.values & ~BINARY) && value(in) == 2 && forced_[in] == ANY) return in;
        }
        graph_.inputsOf(n, [&](int in) { pending.push_back(in); });
    }
    return -1;
}

GroupResult Engine::solve(const Group& group, bool signals, size_t budget) {
    GroupResult result;
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
            if (graph_.nodes[n].var >= 0) variables.push_back(graph_.nodes[n].var);
            graph_.inputsOf(n, [&](int in) { pending.push_back(in); });
        }
        for (const int n : graph_.order) if (stamp_[n] == stamp_counter_) order.push_back(n);
    }
    const size_t start = evaluations_;
    clearAssumptions();
    run(order, false);
    const Tick structural = latest(group).first;
    result.bounded = true;
    if (structural == NEVER) return result;
    result.changes = true;

    struct Choice { std::shared_ptr<const Choice> parent; int node; uint8_t value; };
    struct Open { Tick ceiling; int depth; std::shared_ptr<const Choice> choice; };
    const auto later = [](const Open& a, const Open& b) {
        return a.ceiling != b.ceiling ? a.ceiling < b.ceiling : a.depth < b.depth;
    };
    std::priority_queue<Open, std::vector<Open>, decltype(later)> open(later);
    const auto load = [&](const std::shared_ptr<const Choice>& choice) {
        clearAssumptions();
        for (auto c = choice; c; c = c->parent) if (!force(c->node, c->value)) return false;
        return evaluate(order);
    };
    // A complete final vector, evaluated with nothing else assumed: the value
    // it gives is reached by a real case, so no bound below it exists.
    const auto plain = [&](const std::vector<int8_t>& vector) {
        clearAssumptions();
        for (const int var : variables) force(graph_.variable_node[var], vector[var] ? HIGH : LOW);
        run(order, true);
        return latest(group).first;
    };
    const auto finish = [&] { result.evaluations = evaluations_ - start; clearAssumptions(); return result; };
    Tick attained = NEVER - 1;
    std::vector<int8_t> attaining(graph_.variables.size(), 2);
    if (!variables.empty()) for (int mode = 0; mode < 8; ++mode) {
        std::vector<int8_t> vector(graph_.variables.size(), 2);
        for (const int var : variables) vector[var] = static_cast<int8_t>(mode < 2 ? mode : random_() & 1);
        const auto reached = plain(vector);
        // Even a fully decided input leaves a loop open: no bound exists.
        if (reached == OPEN) { result.bounded = false; return finish(); }
        if (reached > attained) { attained = reached; attaining = std::move(vector); }
    }

    Tick bound = structural;
    std::shared_ptr<const Choice> limiting;
    bool from_vector = false;
    if (load(nullptr)) open.push({latest(group).first, 0, nullptr});
    const Tick unrestricted = open.empty() ? structural : open.top().ceiling;
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
        int branch = -1;
        if (top.ceiling == OPEN) {
            branch = openLoop(critical);
            if (branch < 0) { open.push(top); break; } // nothing left that could close the loop
        } else {
            if (critical >= 0) branch = openSide(critical, signals, opaque);
            if (branch < 0) branch = openJustification(order);
            // A decided path is not enough: a side input that also controls
            // may still settle earlier. Decide everything below the endpoint.
            if (branch < 0 && critical >= 0) branch = openVariable(critical, false);
        }
        if (branch < 0) {
            // One complete final vector below the limiting endpoint.
            // Confirm its value with nothing assumed.
            std::vector<int8_t> vector(graph_.variables.size(), 2);
            for (const int var : variables) {
                const uint8_t decided = forced_[graph_.variable_node[var]];
                vector[var] = decided == HIGH ? int8_t{1} : int8_t{0};
            }
            const auto reached = plain(vector);
            if (reached > attained) { attained = reached; attaining = std::move(vector); }
            from_vector = reached == top.ceiling;
            break;
        }
        for (const uint8_t value : {LOW, HIGH}) {
            const auto child = std::make_shared<const Choice>(Choice{top.choice, branch, value});
            if (load(child)) open.push({latest(group).first, top.depth + 1, child});
        }
    }
    if (bound == OPEN) { result.bounded = false; return finish(); }

    bool sensitized = false;
    if (from_vector) {
        plain(attaining);
        bool opaque = false;
        sensitized = latest(group).second < 0 || (openSide(latest(group).second, true, opaque) < 0 && !opaque);
    } else if (!load(limiting)) {
        clearAssumptions();
        run(order, true);
    }
    result.path = pathTo(latest(group).second);
    if (bound == NEVER) { result.changes = false; return finish(); }
    result.bound = bound;
    result.structural = structural == OPEN ? bound : structural;
    if (from_vector) {
        result.tight = sensitized;
        result.witness = attaining;
    }
    return finish();
}

Engine::Resolved Engine::resolve(const std::vector<int>& needed, size_t work_budget,
                                 std::vector<Fact>& worst, size_t& splits) {
    using Choices = std::vector<std::pair<int, uint8_t>>;
    std::vector<Choices> pending{{}};
    worst.assign(graph_.nodes.size(), Fact{0, NEVER, NEVER});
    std::fill(known_.begin(), known_.end(), ANY);
    if (case_) for (const auto& [n, values] : case_->known) known_[n] &= values;
    size_t cases = 0;
    Resolved outcome = Resolved::Yes;
    while (!pending.empty()) {
        const Choices choices = std::move(pending.back());
        pending.pop_back();
        clearAssumptions();
        bool feasible = true;
        for (const auto& [n, value] : choices) feasible = feasible && force(n, value);
        feasible = feasible && evaluate(graph_.order);
        if (work_ > work_budget) { outcome = Resolved::OutOfWork; break; }
        if (!feasible) continue;
        int unsettled = -1;
        for (const int n : needed) if (fact_[n].settled == OPEN) { unsettled = n; break; }
        if (unsettled < 0) {
            // What holds in every case: any of the values, by the later tick.
            for (size_t n = 0; n < worst.size(); ++n) {
                worst[n].values |= fact_[n].values;
                worst[n].at = std::max(worst[n].at, fact_[n].at);
                worst[n].settled = std::max(worst[n].settled, fact_[n].settled);
            }
            continue;
        }
        const int split = openLoop(unsettled);
        if (split < 0 || choices.size() >= 16 || ++cases > 256) { outcome = Resolved::No; break; }
        ++splits;
        for (const uint8_t value : {LOW, HIGH}) {
            auto next = choices;
            next.emplace_back(split, value);
            pending.push_back(std::move(next));
        }
    }
    clearAssumptions();
    return outcome;
}

} // namespace circuit::analysis::detail::timing
