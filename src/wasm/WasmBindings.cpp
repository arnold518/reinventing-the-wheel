#include "visualizer/VisualizerSession.hpp"

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class WasmVisualizer {
public:
    std::string visualizableTestsJson() const {
        return circuit::visualizer::VisualizerSession::visualizableTestsJson();
    }

    void clearProfileOverrides() { exact_overrides_.clear(); }

    void setProfileOverride(
        const std::string& component_path,
        const std::string& fidelity) {
        if (fidelity != "structural" && fidelity != "behavioral") {
            throw std::invalid_argument(
                "Fidelity must be structural or behavioral");
        }
        exact_overrides_[component_path] = fidelity;
    }

    std::string loadScenario(
        const std::string& registered_test_name,
        const std::string& scenario_key,
        size_t profile_revision) {
        session_ = std::make_unique<circuit::visualizer::VisualizerSession>(
            registered_test_name,
            scenario_key,
            exact_overrides_,
            profile_revision);
        return session_->topologyResponseJson();
    }

    std::string stateJson(size_t index) {
        return session().stateJson(index);
    }

    std::string componentStateJson(
        size_t index,
        const std::string& component_id) {
        return session().componentStateJson(index, component_id);
    }

    std::string topologySliceJson(const emscripten::val& owner_ids) const {
        if (!owner_ids.isArray()) {
            throw std::invalid_argument("ownerIds must be an array");
        }
        const auto length = owner_ids["length"].as<size_t>();
        std::vector<std::string> values;
        values.reserve(length);
        for (size_t index = 0; index < length; ++index) {
            values.push_back(owner_ids[index].as<std::string>());
        }
        return session().topologySliceJson(values);
    }

private:
    circuit::visualizer::VisualizerSession& session() const {
        if (!session_) {
            throw std::logic_error(
                "Load a scenario before requesting simulation data");
        }
        return *session_;
    }

    std::map<std::string, std::string> exact_overrides_;
    std::unique_ptr<circuit::visualizer::VisualizerSession> session_;
};

} // namespace

EMSCRIPTEN_BINDINGS(circuitsim_visualizer) {
    emscripten::class_<WasmVisualizer>("WasmVisualizer")
        .constructor<>()
        .function("visualizableTestsJson", &WasmVisualizer::visualizableTestsJson)
        .function("clearProfileOverrides", &WasmVisualizer::clearProfileOverrides)
        .function("setProfileOverride", &WasmVisualizer::setProfileOverride)
        .function("loadScenario", &WasmVisualizer::loadScenario)
        .function("stateJson", &WasmVisualizer::stateJson)
        .function("componentStateJson", &WasmVisualizer::componentStateJson)
        .function("topologySliceJson", &WasmVisualizer::topologySliceJson);
}
