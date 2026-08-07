#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class Component;

namespace circuit::analysis {

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
};

/**
 * Static, non-mutating delay analysis over the circuit that was actually
 * instantiated. BasicComponent delays are the timing weights. Structural
 * D-flip-flops and compact sequential implementations terminate a path and
 * start a new one at their Q outputs.
 *
 * This is an educational normalized-delay model. It is not a transistor,
 * routing, setup/hold, or FPGA place-and-route model.
 */
class CircuitTimingAnalyzer {
public:
    static CircuitTimingReport analyze(
        const std::shared_ptr<Component>& root);
};

} // namespace circuit::analysis
