#pragma once

#include "analysis/SafeClockTiming.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Component;

namespace circuit::analysis {

// Which sources are allowed to change. The others are treated as having been
// unchanged for long enough that everything depending only on them is quiet.
enum class SettlingSources {
    All,        // root inputs change, and clocked outputs launch
    InputsOnly, // root inputs change; clocked state holds
    StateOnly   // clocked outputs launch; root inputs hold
};

// How final values are split into cases. Every choice gives a sound bound.
enum class SettlingSearch {
    Both,      // Variables first; Signals as well if that leaves the bound open
    Variables, // decide source bits only
    Signals    // also decide internal signals, with their consequences
};

struct SettlingOptions {
    // false: a final source value may be 0, 1, X or Z. No value search is
    //        possible, because an X select keeps both sides of a choice live.
    // true:  ENVIRONMENT PREMISE. The final value of every root input bit and
    //        of every purely clocked output bit is 0 or 1. Earlier values and
    //        every internal wire remain unrestricted.
    bool binary_sources = false;
    SettlingSources sources = SettlingSources::All;
    // Root input pin name -> value that never changes during operation.
    std::map<std::string, uint64_t> fixed_inputs;
    // Root input pin name -> the value it ends at. It still changes; only
    // the value it settles to is stated (for example one operation code).
    std::map<std::string, uint64_t> final_inputs;
    // Root input pin name -> tick of its last transition (default 0).
    std::map<std::string, size_t> input_arrival;
    // Timed evaluations allowed per endpoint. Running out never weakens the
    // result: the bound reported is then the largest one not yet excluded.
    size_t search_budget = 20000;
    SettlingSearch search = SettlingSearch::Both;
    uint64_t seed = 1;
    TimingContracts contracts;
};

struct SettlingEndpoint {
    std::string name;       // a root output pin, or "(captures)"
    bool changes = false;   // false: provably never changes in operation
    size_t structural = 0;  // longest per-bit path, values ignored
    size_t bound = 0;       // proven latest change (captures: plus setup)
    // A final source vector reaches `bound` in the timed-value model, so no
    // further case split can lower it. It does not claim that the simulator
    // shows a change that late for some previous state.
    bool tight = false;
    size_t evaluations = 0;
    std::vector<std::string> path;              // pin[bit] along the limiting path
    std::map<std::string, uint64_t> witness;    // source pin ID -> final value
};

struct SettlingReport {
    bool valid = false;
    size_t structural = 0, bound = 0;
    size_t bit_count = 0, variable_count = 0;
    std::vector<SettlingEndpoint> endpoints;
    std::vector<std::string> premises, diagnostics;
};

/**
 * Upper bound on the last tick at which each root output (and each clocked
 * capture) can still change after its sources stop changing.
 *
 * Basis. Every evaluated part publishes f(inputs at t) at exactly t + delay,
 * so once a part's inputs are constant from tick s, its output is constant
 * from s + delay. Two sound refinements are applied per bit:
 *   - a bit that depends only on unchanging sources never changes;
 *   - an input holding a controlling value (0 for AND/NAND, 1 for OR/NOR)
 *     from tick s fixes the output from s + delay, whatever the other input
 *     does. X and Z never control, exactly as in the gates.
 * With binary_sources, final values are split into cases and the largest
 * case is reported. A case fixes the final value of a source bit or of an
 * internal signal; what follows from it (AND = 1 needs both inputs 1, and so
 * on) is added, and a case that contradicts itself is dropped because no
 * input can produce it. The two cases of one bit always cover every input,
 * so the search can only remove paths that no input uses. It never adds
 * delay, and stopping it early leaves a larger bound, not a wrong one.
 *
 * Fails closed: a part without a timing contract, a combinational cycle, a
 * derived clock or a falling-edge contract makes the report invalid. Bounds
 * latest settling only; it says nothing about hold or earliest change.
 */
SettlingReport analyzeSettling(const std::shared_ptr<Component>& root,
                               const SettlingOptions& options = {});

} // namespace circuit::analysis
