#include "visualizer/VisualizerSession.hpp"
#include "runtime/RecordedGuestFixtures.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/FidelityPresets.hpp"
#include "tests/GuestScenarioTests.hpp"

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <map>
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class WasmVisualizer {
public:
    std::string runRecordedGuestJson(const std::string& name, bool caches, const std::string& memory, unsigned max_cycles) {
        return circuit::runtime::runRecordedGuestFixtureJson(name,caches,memory,max_cycles,{},100000,false,true,0,0,
            fidelity_preset_);
    }

    std::string runRecordedGuestSurfaceJson(const std::string& name,unsigned width,unsigned height,unsigned max_cycles) {
        return circuit::runtime::runRecordedGuestFixtureJson(name,false,{},max_cycles,{},100000,false,true,width,height,
            fidelity_preset_);
    }

    std::string visualizableTestsJson() const {
        return circuit::visualizer::VisualizerSession::visualizableTestsJson();
    }

    void clearProfileOverrides() { exact_overrides_.clear(); }
    void setFidelityProfile(const std::string& name){
        const auto names=circuit::fidelityPresetNames();
        if(std::find(names.begin(),names.end(),name)==names.end())throw std::invalid_argument("Unknown fidelity profile: "+name);
        fidelity_preset_=name;
    }

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

    // The save the next game-replay scenario starts from (bytes of a long
    // recording's save file, already decompressed) and how many cycles it runs.
    void setGameReplay(const std::string& fixture, const emscripten::val& bytes, unsigned cycles) {
        auto save = std::make_shared<std::vector<uint8_t>>(emscripten::convertJSArrayToNumberVector<uint8_t>(bytes));
        setGuestReplay({fixture, std::move(save), cycles});
    }

    // A recorded game's session: simulate more cycles from where it is, or
    // jump to another frame's save (decompressed bytes). Both return the
    // current stretch's timeline.
    std::string gameAdvance(unsigned cycles) { return session().gameAdvanceJson(cycles); }
    std::string gameFastForward(unsigned cycles) { return session().gameFastForwardJson(cycles); }
    std::string gameJump(const emscripten::val& bytes) {
        return session().gameJumpJson(std::make_shared<const std::vector<uint8_t>>(
            emscripten::convertJSArrayToNumberVector<uint8_t>(bytes)));
    }

    std::string loadScenario(
        const std::string& registered_test_name,
        const std::string& scenario_key,
        size_t profile_revision) {
        session_ = std::make_unique<circuit::visualizer::VisualizerSession>(
            registered_test_name,
            scenario_key,
            exact_overrides_,
            profile_revision,fidelity_preset_);
        return session_->topologyResponseJson();
    }

    // The parts the scenario would have with the current overrides and
    // preset, built without simulating (the explorer's preview).
    std::string previewComponentsJson(const std::string& registered_test_name) const {
        return circuit::visualizer::VisualizerSession::previewComponentsJson(
            registered_test_name, exact_overrides_, fidelity_preset_);
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
    // Empty until the page chooses one: recorded guests then run their own profile.
    std::string fidelity_preset_;
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
        .function("setGameReplay", &WasmVisualizer::setGameReplay)
        .function("gameAdvance", &WasmVisualizer::gameAdvance)
        .function("gameFastForward", &WasmVisualizer::gameFastForward)
        .function("gameJump", &WasmVisualizer::gameJump)
        .function("previewComponentsJson", &WasmVisualizer::previewComponentsJson)
        .function("setFidelityProfile", &WasmVisualizer::setFidelityProfile)
        .function("stateJson", &WasmVisualizer::stateJson)
        .function("exampleRowsJson", &WasmVisualizer::exampleRowsJson)
        .function("componentStateJson", &WasmVisualizer::componentStateJson)
        .function("memoryPageJson", &WasmVisualizer::memoryPageJson)
        .function("topologySliceJson", &WasmVisualizer::topologySliceJson);
}
