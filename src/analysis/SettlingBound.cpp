#include "analysis/CircuitTimingAnalyzer.hpp"
#include "SettlingGraph.hpp"
#include "basic/PinBase.hpp"
#include <algorithm>
#include <queue>
#include <random>
#include <stdexcept>

namespace circuit::analysis {
namespace {

using namespace detail::settling;

// Timed evaluation of one case, and the search over cases.
class Search {
    const SettlingOptions& options_;
    const Graph& graph_;

    std::vector<int8_t> value_;
    std::vector<long long> time_;
    std::vector<int> previous_, stamp_;
    int stamp_counter_ = 0;
    size_t evaluations_ = 0;

    // The case being examined: final values assumed for some bits.
    std::vector<int8_t> forced_;
    std::vector<int> touched_, unjustified_;
    std::vector<double> weight_;
    bool conflict_ = false;

    // The timed-value rules. `values == false` ignores every value (per-bit
    // structural longest path). Otherwise the assumed final values of the
    // current case are used, and a contradiction with them is recorded.
    void run(const std::vector<int>& order, bool values) {
        ++evaluations_;
        conflict_ = false;
        unjustified_.clear();
        for (const int n : order) {
            const auto& x = graph_.nodes[n];
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
                const auto& hub = graph_.hubs[x.hub];
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
            const auto& x = graph_.nodes[n];
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
            const auto& x = graph_.nodes[n];
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
            if (graph_.nodes[n].var >= 0 && forced_[n] == UNKNOWN) return n;
            graph_.inputsOf(n, [&](int in) { pending.push_back(in); });
        }
        return -1;
    }
    // What to decide next on the limiting path: a side input whose value is
    // still open. Deciding it either removes the path or keeps it. With
    // `internal` the side input itself is decided; otherwise a case variable
    // below it. `opaque` reports open side inputs that may be X or Z.
    int openSide(int critical, bool internal, bool& opaque) {
        for (int n = critical; n >= 0; n = previous_[n]) {
            const auto& node = graph_.nodes[n];
            if (node.kind != Kind::And && node.kind != Kind::Nand && node.kind != Kind::Or && node.kind != Kind::Nor)
                continue;
            const int side = previous_[n] == node.a ? node.b : node.a;
            if (value_[side] != UNKNOWN) continue;
            if (!graph_.binary[side]) { opaque = true; continue; }
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
            if (graph_.nodes[n].var >= 0 && forced_[n] == UNKNOWN && (best < 0 || w > weight_[best])) best = n;
            graph_.inputsOf(n, [&](int in) { if (value_[in] == UNKNOWN) weight_[in] += w; });
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
                if (graph_.nodes[n].var >= 0) variables.push_back(graph_.nodes[n].var);
                graph_.inputsOf(n, [&](int in) { pending.push_back(in); });
            }
            for (const int n : graph_.order) if (stamp_[n] == stamp_counter_) order.push_back(n);
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
            for (const int var : variables) force(graph_.variable_node[var], vector[var]);
            run(order, true);
            return latest(group).first;
        };
        long long attained = STABLE - 1;
        std::vector<int8_t> attaining(graph_.variables.size(), UNKNOWN);
        std::mt19937_64 random(options_.seed);
        if (!variables.empty()) for (int mode = 0; mode < 8; ++mode) {
            std::vector<int8_t> vector(graph_.variables.size(), UNKNOWN);
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
                std::vector<int8_t> vector(graph_.variables.size(), UNKNOWN);
                for (const int var : variables) {
                    const int8_t decided = forced_[graph_.variable_node[var]];
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
        for (int n = critical; n >= 0; n = previous_[n]) result.path.push_back(graph_.label(n));
        std::reverse(result.path.begin(), result.path.end());
        if (from_vector) {
            result.tight = sensitized;
            for (const int var : variables) if (graph_.variables[var].second < 64)
                result.witness[graph_.variables[var].first->getID()] |= uint64_t(attaining[var]) << graph_.variables[var].second;
        }
        clearCase();
        return result;
    }

public:
    Search(const SettlingOptions& options, const Graph& graph)
        : options_(options), graph_(graph), value_(graph.nodes.size(), UNKNOWN),
          time_(graph.nodes.size(), STABLE), previous_(graph.nodes.size(), -1), stamp_(graph.nodes.size(), 0),
          forced_(graph.nodes.size(), UNKNOWN), weight_(graph.nodes.size(), 0) {}

    // Two ways of choosing cases; both are sound, so the lower result
    // stands. The second runs only if the first leaves the bound open.
    SettlingEndpoint solve(const Group& group) {
        const bool both = options_.search == SettlingSearch::Both;
        auto result = solve(group, options_.search == SettlingSearch::Signals,
                            both ? options_.search_budget / 2 : options_.search_budget);
        if (!both || !result.changes || result.tight) return result;
        auto other = solve(group, true, options_.search_budget - result.evaluations);
        other.evaluations += result.evaluations;
        if (!other.changes || other.bound < result.bound || (other.bound == result.bound && other.tight))
            return other;
        result.evaluations = other.evaluations;
        return result;
    }

    void fill(SettlingReport& report) {
        run(graph_.order, true);
        for (const auto& [n, value, id] : graph_.required)
            if (time_[n] != STABLE || value_[n] != value) {
                report.diagnostics.push_back(id + ": contract requires the fixed value " + std::to_string(value)
                                             + "; state it with fixed_inputs");
                return;
            }
        for (const auto& group : graph_.groups) {
            if (group.endpoints.empty()) continue;
            report.endpoints.push_back(solve(group));
            report.structural = std::max(report.structural, report.endpoints.back().structural);
            report.bound = std::max(report.bound, report.endpoints.back().bound);
        }
        report.bit_count = graph_.nodes.size();
        report.variable_count = graph_.variables.size();
        report.premises = {
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
        report.valid = true;
    }
};

} // namespace

SettlingReport CircuitTimingAnalyzer::settling(const std::shared_ptr<Component>& root, const SettlingOptions& options) {
    SettlingReport report;
    if (!root) {
        report.diagnostics.push_back("No circuit");
        return report;
    }
    Graph graph;
    if (!buildGraph(root, options, graph, report.diagnostics)) return report;
    try {
        Search(options, graph).fill(report);
    } catch (const std::exception& error) {
        report.diagnostics.push_back(error.what());
        report.valid = false;
        report.endpoints.clear();
    }
    return report;
}

} // namespace circuit::analysis
