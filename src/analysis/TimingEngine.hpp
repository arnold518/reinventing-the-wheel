#pragma once

#include "TimingGraph.hpp"
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

// The one evaluation every timing question uses.
//
// Each bit carries a Fact: from tick `at` on its value lies in `values`, and
// it does not change after tick `settled`. Facts start empty of information
// and are only ever tightened by rules that follow from the parts' behaviour:
//   - a part publishes f(inputs at t) at exactly t + delay, so an output is
//     settled one delay after all its inputs are;
//   - a gate's output set is the image of its input sets, valid one delay
//     after the inputs' sets are; a single possible value means "settled";
//   - a controlling input (0 on AND, 1 on OR) decides the output alone.
// Tightening is repeated until nothing moves, so loops need no special
// treatment: a loop settles when some value on it is decided, and otherwise
// stays open.
namespace circuit::analysis::detail::timing {

struct Fact {
    uint8_t values = ANY;
    Tick at = NEVER;
    Tick settled = OPEN;
    bool operator==(const Fact&) const = default;
};

enum PartMode : uint8_t { AnyMode, Empty, Full };

// What a caller knows beyond the circuit itself, for one of a set of cases
// that together cover every situation (for example the reachable states of a
// memory handshake).
struct Case {
    std::vector<std::pair<int, uint8_t>> known; // source bit -> set of final values
    std::vector<uint8_t> part_mode;             // per Graph::parts entry
    bool binary_parts = false;                  // part and register-file outputs are 0 or 1
};

struct GroupResult {
    bool bounded = false;  // false: a loop could not be settled
    bool changes = false;
    Tick structural = 0, bound = 0;
    bool tight = false;
    size_t evaluations = 0;
    std::vector<int> path;
    std::vector<int8_t> witness; // per case variable: 0, 1 or 2 (not decided)
};

class Engine {
public:
    explicit Engine(const Graph& graph, uint64_t seed = 1);

    void setCase(const Case* external) { case_ = external; }
    size_t work() const { return work_; }
    size_t evaluations() const { return evaluations_; }

    // Everything, with no final value assumed.
    void evaluateAll();
    const Fact& fact(int n) const { return fact_[n]; }
    std::vector<int> pathTo(int n) const;

    // The latest change of a group, lowered by splitting final values into
    // cases. `signals`: also decide internal signals, not only source bits.
    GroupResult solve(const Group& group, bool signals, size_t budget);

    // Split into cases until every listed bit is settled. `worst` receives,
    // per bit, what holds in every case.
    enum class Resolved { Yes, No, OutOfWork };
    Resolved resolve(const std::vector<int>& needed, size_t work_budget, std::vector<Fact>& worst, size_t& splits);

private:
    const Graph& graph_;
    const Case* case_ = nullptr;
    std::mt19937_64 random_;

    std::vector<Fact> fact_;
    std::vector<uint8_t> raw_;      // value set from the inputs alone
    std::vector<uint8_t> known_;    // from the external case
    std::vector<int> previous_, stamp_, cone_;
    std::vector<uint8_t> queued_;
    std::vector<int> queue_;
    int stamp_counter_ = 0, cone_counter_ = 0;
    size_t evaluations_ = 0, work_ = 0;

    // The case being examined: final values assumed for some bits.
    std::vector<uint8_t> forced_;
    std::vector<int> touched_, unjustified_;
    std::vector<double> weight_;
    bool conflict_ = false;

    int value(int n) const; // 0, 1, or 2 when not decided
    void apply(int n, uint8_t values, Tick at, Tick settled, int previous);
    bool refine(int n, bool values);
    void refineGate(int n, const Node& x, bool values);
    void refinePart(int n, const Node& x, bool values);
    void run(const std::vector<int>& order, bool values);
    std::pair<Tick, int> latest(const Group& group) const;

    bool force(int start, uint8_t start_value);
    int imply();
    void clearAssumptions();
    bool evaluate(const std::vector<int>& order);
    int openVariable(int start, bool open_only);
    int openSide(int critical, bool signals, bool& opaque);
    int openJustification(const std::vector<int>& order);
    int openLoop(int unsettled);
};

} // namespace circuit::analysis::detail::timing
