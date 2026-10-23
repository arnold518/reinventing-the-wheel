#pragma once

#include "simulator/SimulationTime.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Component;
class IOComponent;
class PinBase;
class Simulator;

namespace circuit::visualizer {
// Include internal device observations in the visual timeline even when a
// fused endpoint's external bus pins did not change. No simulated events added.
std::vector<SimulationTime> memoryActivityTimestamps(const std::shared_ptr<Component>&);
// Host-side observation, not a component/controller. Reconstructs the blocking
// bus at clock edges from recorded pins, including same-edge bypass and a held
// response. Reads never modify device state, simulator time, or event counts.
class MemoryInspector {
public:
    MemoryInspector(std::shared_ptr<Component>, const Simulator&);
    std::string stateJson(size_t index, SimulationTime time, const std::string& scenario = {},
                          int64_t address = -1, size_t count = 64);
private:
    struct Edge {
        SimulationTime time;
        std::optional<SimulationTime> pending, completed;
    };
    // One memory shared by instruction fetches and data accesses: the two-port
    // arbiter in front of it tells, at each accepting edge, which port the
    // request came from.
    struct Access {
        SimulationTime time;
        bool data;
        uint64_t address;
    };
    std::shared_ptr<IOComponent> component_, endpoint_, arbiter_;
    const Simulator& simulator_;
    size_t revision_ = size_t(-1);
    std::vector<Edge> edges_;
    std::vector<Access> accesses_;
    bool rv32im_ = false;
    std::shared_ptr<PinBase> pin(const std::shared_ptr<IOComponent>&, const std::string&) const;
    std::string value(const std::shared_ptr<IOComponent>&, const std::string&, SimulationTime) const;
    void rebuildTransactions();
};
}
