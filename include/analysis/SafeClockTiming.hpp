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

namespace circuit::analysis {

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

// Conservative sufficient period, NOT an exact minimum or a physical Fmax.
// SafeUnderContracts is conditional on the supplied contracts being sound.
// Unknown/TimingViolation never contain a usable period. Does not run or mutate
// the circuit. Initial reset/settling precedes the analyzed steady operation.
ClockTimingReport computeSafeClockPeriod(
    const std::shared_ptr<Component>& root,
    const ClockSpecification& clock,
    const TimingContracts& contracts = {});

} // namespace circuit::analysis
