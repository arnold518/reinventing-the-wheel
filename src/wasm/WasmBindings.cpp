#include "visualizer/VisualizerSession.hpp"
#include "runtime/RecordedGuestFixtures.hpp"
#include "components/TimingProfile.hpp"

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <map>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class WasmVisualizer {
public:
    std::string runRecordedGuestJson(const std::string& name, bool caches, bool dram, unsigned max_cycles) {
        return circuit::runtime::runRecordedGuestFixtureJson(name,caches,dram,max_cycles);
    }

    std::string runRecordedGuestSurfaceJson(const std::string& name,unsigned width,unsigned height,unsigned max_cycles) {
        return circuit::runtime::runRecordedGuestFixtureJson(name,false,false,max_cycles,{},100000,false,true,width,height);
    }

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

    // The timing profile for the next loadScenario (the whole library's
    // setting; this worker holds one session at a time).
    void setTimingProfile(const std::string& name) { circuit::timing::setActiveTimingProfile(name); }

    std::string timingProfileJson() const {
        const auto quote = [](const std::string& text) {
            std::string out = "\"";
            for (const char c : text) {
                if (c == '"' || c == '\\') out += '\\';
                out += c;
            }
            return out + "\"";
        };
        const auto& profile = circuit::timing::activeTimingProfile();
        std::string available = "[";
        for (const auto& name : circuit::timing::timingProfileNames())
            available += (available.size() > 1 ? "," : "") + quote(name);
        return "{\"name\":" + quote(profile.name) + ",\"description\":" + quote(profile.description)
            + ",\"tickPicoseconds\":" + std::to_string(profile.tick_picoseconds)
            + ",\"available\":" + available + "]}";
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
    std::string exampleRowsJson() const { return session().exampleRowsJson(); }

    std::string componentStateJson(
        size_t index,
        const std::string& component_id) {
        return session().componentStateJson(index, component_id);
    }

    std::string memoryPageJson(size_t index, const std::string& component_id, double address, size_t count) {
        if (!std::isfinite(address) || std::floor(address) != address || address < -1 || address > UINT32_MAX)
            throw std::invalid_argument("Invalid memory page address");
        return session().componentStateJson(index, component_id, static_cast<int64_t>(address), count);
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
        .function("runRecordedGuestJson", &WasmVisualizer::runRecordedGuestJson)
        .function("runRecordedGuestSurfaceJson", &WasmVisualizer::runRecordedGuestSurfaceJson)
        .function("visualizableTestsJson", &WasmVisualizer::visualizableTestsJson)
        .function("clearProfileOverrides", &WasmVisualizer::clearProfileOverrides)
        .function("setProfileOverride", &WasmVisualizer::setProfileOverride)
        .function("setTimingProfile", &WasmVisualizer::setTimingProfile)
        .function("timingProfileJson", &WasmVisualizer::timingProfileJson)
        .function("loadScenario", &WasmVisualizer::loadScenario)
        .function("stateJson", &WasmVisualizer::stateJson)
        .function("exampleRowsJson", &WasmVisualizer::exampleRowsJson)
        .function("componentStateJson", &WasmVisualizer::componentStateJson)
        .function("memoryPageJson", &WasmVisualizer::memoryPageJson)
        .function("topologySliceJson", &WasmVisualizer::topologySliceJson);
}
