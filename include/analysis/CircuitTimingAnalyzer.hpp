#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Component;
class IOComponent;

// One timing analysis for built circuits. It reads the component tree, uses
// one contract per indivisible part, and answers two questions from the same
// model:
//   - settling:  the last tick at which each output can still change;
//   - safeClock: a clock period under which every capture is met.
// These are normalized simulator ticks, not physical delays.
namespace circuit::analysis {

// ---- Part contracts and the clock question ---------------------------------

enum class ClockEdge { Rising, Falling };
enum class ClockTimingStatus { SafeUnderContracts, TimingViolation, Unknown };
const char* clockTimingStatusName(ClockTimingStatus status);

struct DelayBounds {
    size_t minimum = 0;
    size_t maximum = 0;
};
struct TimingArc {
    std::string input, output;
    DelayBounds delay;
};
struct ClockCapture {
    std::string pin;
    ClockEdge edge = ClockEdge::Rising;
    size_t setup = 0, hold = 0;
};
struct ClockLaunch {
    std::string pin;
    ClockEdge edge = ClockEdge::Rising;
    DelayBounds delay;
};

// A trusted, all-state contract for ONE concrete implementation, not merely its
// functional component family. Delays bound earliest and latest transitions,
// including glitches, as delivered on simulator wires (not immediate UI pin
// caches). All pins must be classified. No implicit false paths.
struct ClockComponentContract {
    std::string evidence;
    std::string clock_pin;
    std::vector<TimingArc> arcs;
    std::vector<ClockCapture> captures;
    std::vector<ClockLaunch> launches;
    std::map<std::string, uint64_t> fixed_inputs;
    std::map<std::string, uint64_t> constant_outputs;
    std::vector<std::string> stable_outputs; // May be an unknown but unchanging value.
    std::vector<std::string> unused_inputs;
    size_t minimum_high_ticks = 0, minimum_low_ticks = 0;
};
struct ClockInputConstraint {
    std::string pin; // Fully qualified external input pin ID.
    std::optional<ClockEdge> edge; // No edge means fixed throughout operation.
    DelayBounds delay; // Input transition window relative to edge.
    uint64_t fixed_value = 0;
    // Required for changing external inputs in binary-startup mode. This is
    // an explicit environment promise, not inferred from a current pin value.
    bool binary_values = false;
};
struct ClockSpecification {
    std::string clock_pin; // Fully qualified external clock input pin ID.
    std::vector<ClockInputConstraint> inputs;
    std::vector<ClockCapture> outputs; // Fully qualified external output pin IDs.
    // One ideal, directly distributed, 50% duty-cycle clock. Strict separation
    // avoids relying on simulator ordering for coincident data/clock events.
    size_t event_separation_ticks = 1;
    size_t maximum_nodes = 500000, maximum_edges = 2000000;
    size_t maximum_hierarchy_depth = 256;
    size_t maximum_state_cases = 64;
    size_t maximum_state_evaluations = 10000000;
    // Opt-in: inspect provisioned FF memory, use reset storage seeds, and prove
    // preservation of binary captures/controller decisions. Reset and initial
    // settling remain environment premises. Unrestricted mode stays default.
    bool binary_startup = false;
};
struct TimingContracts {
    std::function<std::optional<ClockComponentContract>(const IOComponent&)> model;
};
struct ClockTimingReport {
    ClockTimingStatus status = ClockTimingStatus::Unknown;
    std::optional<size_t> safe_period_ticks;
    size_t node_count = 0, edge_count = 0, capture_count = 0;
    size_t checked_setup_relationships = 0, checked_hold_relationships = 0;
    std::string limiting_constraint;
    std::vector<std::string> critical_path;
    // Closed directed walk in the conservative dependency graph, not proof of
    // an oscillating input pattern. Kept separate from a timed critical path.
    std::vector<std::string> dependency_cycle;
    size_t state_case_count = 0, checked_state_cases = 0;
    size_t state_evaluations = 0;
    bool binary_startup_verified = false;
    // Bit/case checks, including quantified controller decisions, not capacity.
    size_t binary_capture_bits = 0;
    std::vector<std::string> binary_evidence;
    std::vector<std::string> state_evidence;
    std::vector<std::string> assumptions, contracts, diagnostics;
};

// ---- The settling question --------------------------------------------------

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
    // Root input pins that hold while the others change: their value is not
    // known, but they do not move. Used to time one input at a time.
    std::vector<std::string> held_inputs;
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

// ---- Characterizing one part ----------------------------------------------------

struct PartDelay {
    std::string output;   // output pin
    bool changes = false; // false: this cause never changes the output
    size_t delay = 0;     // proven latest change, in ticks
    bool tight = false;
};
// The delays of one part as seen from outside: what a compact form of the
// same part has to reproduce.
struct PartTiming {
    bool valid = false;
    bool clocked = false;                    // the part captures or launches on a clock
    std::vector<PartDelay> input_to_output;  // inputs change while state holds
    std::vector<PartDelay> clock_to_output;  // state launches while inputs hold
    size_t input_to_capture = 0;             // latest arrival at a clocked capture, setup included
    size_t state_to_capture = 0;
    // One number for a part that has a single delay: the latest change of any
    // output from any cause.
    size_t delay = 0;
    std::vector<std::string> premises, diagnostics;
};

class CircuitTimingAnalyzer {
public:
    // Both settling questions for one part: inputs changing with state held,
    // and state launching with inputs held. `options.sources` is ignored.
    static PartTiming characterize(const std::shared_ptr<Component>& root,
                                   const SettlingOptions& options = {});

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
    static SettlingReport settling(const std::shared_ptr<Component>& root,
                                   const SettlingOptions& options = {});

    // Conservative sufficient period, NOT an exact minimum or a physical Fmax.
    // SafeUnderContracts is conditional on the supplied contracts being sound.
    // Unknown/TimingViolation never contain a usable period. Does not run or mutate
    // the circuit. Initial reset/settling precedes the analyzed steady operation.
    static ClockTimingReport safeClock(const std::shared_ptr<Component>& root,
                                       const ClockSpecification& clock,
                                       const TimingContracts& contracts = {});
};

} // namespace circuit::analysis
