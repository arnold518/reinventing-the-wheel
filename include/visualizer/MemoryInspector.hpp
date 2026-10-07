#pragma once
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
std::vector<size_t> memoryActivityTimestamps(const std::shared_ptr<Component>&);
// Host-side observation, not a component/controller. Reconstructs the blocking
// bus at clock edges from recorded pins, including same-edge bypass and a held
// response. Reads never modify device state, simulator time, or event counts.
class MemoryInspector {
public:
    MemoryInspector(std::shared_ptr<Component>, const Simulator&);
    std::string stateJson(size_t index, size_t time, const std::string& scenario = {},
                          int64_t address = -1, size_t count = 64);
private:
    struct Edge {
        size_t time;
        std::optional<size_t> pending, completed;
    };
    std::shared_ptr<IOComponent> component_, endpoint_;
    const Simulator& simulator_;
    size_t revision_ = size_t(-1);
    std::vector<Edge> edges_;
    std::shared_ptr<PinBase> pin(const std::shared_ptr<IOComponent>&, const std::string&) const;
    std::string value(const std::shared_ptr<IOComponent>&, const std::string&, size_t) const;
    void rebuildTransactions();
};
}
