#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Component;
class PinBase;
class SimulationTest;
class Simulator;
class WireBase;

namespace circuit::visualizer {

// Portable visualizer-facing facade shared by the native parity tests and the
// WebAssembly binding.  It intentionally returns the same JSON contracts as
// visualizer-v2/server.py while keeping HTTP, persistence, and browser policy
// outside the simulation engine.
class VisualizerSession {
public:
    VisualizerSession(
        std::string registered_test_name,
        std::string scenario_key,
        std::map<std::string, std::string> exact_overrides = {},
        size_t profile_revision = 0);
    ~VisualizerSession();

    VisualizerSession(const VisualizerSession&) = delete;
    VisualizerSession& operator=(const VisualizerSession&) = delete;

    static std::string visualizableTestsJson();

    std::string topologyResponseJson();
    std::string topologySliceJson(const std::vector<std::string>& owner_ids) const;
    std::string stateJson(size_t index);
    std::string componentStateJson(size_t index, const std::string& component_id);
    // Verified settled scenario rows, not inferred expectations from pin history.
    std::string exampleRowsJson() const;

    const std::string& scenarioKey() const noexcept { return scenario_key_; }
    const std::string& sessionId() const noexcept { return session_id_; }
    const std::string& layoutKey() const noexcept { return layout_key_; }

private:
    struct ComponentRecord;
    struct PinRecord;
    struct WireRecord;

    void buildTopology();
    size_t clampedIndex(size_t index) const;
    std::string pinValue(const std::shared_ptr<PinBase>& pin) const;
    std::string portsJson(
        const std::string& component_id,
        const std::vector<std::string>& names) const;

    std::string registered_test_name_;
    std::string scenario_key_;
    std::string session_id_;
    std::string layout_key_;
    std::map<std::string, std::string> exact_overrides_;
    size_t profile_revision_ = 0;
    bool profile_editable_ = false;

    std::unique_ptr<SimulationTest> test_;
    std::shared_ptr<Component> root_;
    Simulator* simulator_ = nullptr;
    std::vector<size_t> timestamps_;

    std::vector<ComponentRecord> components_;
    std::vector<PinRecord> pins_;
    std::vector<WireRecord> wires_;
    std::map<std::string, size_t> component_index_by_id_;
    std::map<std::string, std::vector<size_t>> pin_indices_by_component_;
    std::map<std::string, std::vector<size_t>> wire_indices_by_owner_;
    std::map<std::pair<std::string, std::string>, size_t>
        pin_index_by_component_name_;
};

} // namespace circuit::visualizer
