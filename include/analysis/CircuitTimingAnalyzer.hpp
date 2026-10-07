#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <optional>
#include <utility>

class Component;
class IOComponent;

namespace circuit::analysis {

// A compact stateful block may ALSO have asynchronous input-to-output arcs.
struct ComponentTimingModel {
    std::vector<std::pair<std::string, std::string>> combinational_arcs;
    std::vector<std::string> capture_inputs;
    std::vector<std::string> launch_outputs;
    size_t clock_to_output = 1;
    size_t capture_allowance = 1;
};
struct CircuitTimingOptions {
    std::function<std::optional<ComponentTimingModel>(const IOComponent&)> model;
};

struct TimingPathPoint {
    std::string component_id;
    std::string component_type;
    size_t component_delay = 0;
    size_t arrival_time = 0;
    bool sequential_boundary = false;
};

struct CircuitTimingReport {
    bool valid = true;
    bool combinational_cycle_detected = false;
    size_t critical_path_delay = 0;
    size_t combinational_component_count = 0;
    size_t sequential_boundary_count = 0;
    size_t connection_count = 0;
    std::vector<TimingPathPoint> critical_path;
    std::string limitation;
    std::vector<std::string> diagnostics;
};

/**
 * Non-mutating pin-arc analysis of the instantiated circuit. BasicComponent
 * delays weight combinational arcs. Registers terminate captures and launch
 * independent output paths; asynchronous read arcs remain visible. Clocked
 * black boxes without a model fail closed. Operating-mode constraints may be
 * supplied explicitly, and must be reported by the caller.
 *
 * This is an educational normalized-delay model. It is not a transistor,
 * routing, setup/hold, or FPGA place-and-route model.
 * For a fail-closed sufficient-period analysis with explicit edge/min/max
 * contracts, use computeSafeClockPeriod in analysis/SafeClockTiming.hpp.
 */
class CircuitTimingAnalyzer {
public:
    static CircuitTimingReport analyze(
        const std::shared_ptr<Component>& root,
        const CircuitTimingOptions& options = {});
};

} // namespace circuit::analysis
