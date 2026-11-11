// Runs one recorded game on one machine, every memory/I-O access checked by
// the recorded runner, and prints its result as JSON.
//
//   system_experiment --guest NAME [--machine NAME] [--memory ff|sram|dram|cached-ff|cached-sram|cached-dram]
//                     [--fidelity compact|logic|visual] [--period TICKS]
//                     [--recording NEW.json] [--max-cycles N]
//
// The timing profile is CIRCUITSIM_TIMING_PROFILE (unit by default). The
// period defaults to 1000 ticks; the fixture's input events keep their cycle
// and move to one tick after the falling edge of the new period. --recording
// writes the visualizer's presentation-checkpoint recording.
#include "runtime/RecordedGuestFixtures.hpp"
#include "system/MemorySubsystem.hpp"
#include "visualizer/VisualizerSession.hpp"
#include "components/TimingProfile.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>

namespace {
uint64_t number(const std::string& text) {
    uint64_t value = 0;
    const auto r = std::from_chars(text.data(), text.data() + text.size(), value);
    if (r.ec != std::errc{} || r.ptr != text.data() + text.size()) throw std::invalid_argument("Not a number: " + text);
    return value;
}
}

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> a{{"--machine", ""}, {"--memory", "ff"}, {"--fidelity", "compact"}, {"--period", "1000"}};
        for (int i = 1; i < argc; i += 2) {
            if (i + 1 >= argc) throw std::invalid_argument(std::string("Missing value for ") + argv[i]);
            a[argv[i]] = argv[i + 1];
        }
        if (!a.contains("--guest")) throw std::invalid_argument("Usage: system_experiment --guest NAME [--machine NAME] [--memory ff|sram|dram|cached-ff|cached-sram|cached-dram] [--fidelity compact|logic|visual] [--period TICKS] [--recording NEW.json] [--max-cycles N]");
        auto f = circuit::runtime::recordedGuestFixture(a.at("--guest"));
        auto& o = f.options;
        if (!a.at("--machine").empty()) o.system = a.at("--machine");
        const auto& memory = a.at("--memory");
        const std::string technology = memory.starts_with("cached-") ? memory.substr(7) : memory;
        if (technology != "ff" && technology != "sram" && technology != "dram") throw std::invalid_argument("Unknown memory " + memory);
        o.machine.caches = memory.starts_with("cached-");
        o.machine.memory = circuit::system::mainMemoryFromName(technology);
        o.fidelity_profile = a.at("--fidelity") == "compact" ? "" : a.at("--fidelity");
        o.compact_children = true;
        if (a.contains("--max-cycles")) o.max_cycles = number(a.at("--max-cycles"));
        const SimulationTime period = number(a.at("--period"));
        if (period < 100 || period % 2) throw std::invalid_argument("The period must be even and at least 100 ticks");
        // Re-time the inputs: same cycle, one tick after the new falling edge.
        circuit::runtime::retime(f, period);
        o.progress = [](uint64_t cycles, uint64_t retired, uint64_t frames) {
            std::cerr << cycles << " cycles, " << retired << " retired, " << frames << " presented\n";
        };
        std::unique_ptr<circuit::visualizer::VisualizerSession> view;
        std::string topology, scopes;
        std::vector<std::string> checkpoints;
        size_t retained = 0;
        const bool record = a.contains("--recording");
        if (record) {
            if (std::filesystem::exists(a.at("--recording"))) throw std::invalid_argument("Recording output already exists");
            o.presentation_observer = [&](const auto& root, const Simulator& sim, SimulationTime time) {
                if (!view) view = std::make_unique<circuit::visualizer::VisualizerSession>(root, sim, "recorded-game-" + f.name);
                auto checkpoint = view->currentCheckpointJson(time);
                if (topology.empty()) { topology = view->topologyResponseJson(); scopes = view->topologySlicesJson(); retained = topology.size() + scopes.size(); }
                retained += checkpoint.size();
                if (retained > 48 * 1024 * 1024 || checkpoints.size() >= 128) throw std::runtime_error("Recording budget exhausted");
                checkpoints.push_back(std::move(checkpoint));
            };
        }
        const auto result = circuit::runtime::runRecordedGuest(f.elf, f.input, o);
        auto json = circuit::runtime::recordedGuestJson(result);
        json.insert(1, "\"guest\":\"" + f.name + "\",\"system\":\"" + o.system + "\",\"memory\":\"" + memory + "\",\"fidelity\":\"" +
                           a.at("--fidelity") + "\",\"timingProfile\":\"" + circuit::timing::activeTimingProfile().name + "\",\"elfSha256\":\"" +
                           f.elf_sha256 + "\",\"scriptSha256\":\"" + f.script_sha256 + "\",");
        // How the main memory was set up for this clock (SRAM wait states,
        // DRAM row size and refresh), from the same rule the machine used.
        if (technology != "ff") {
            const auto setup = circuit::system::memorySubsystemFromParameters(
                {{"technology", technology}, {"clock_period", std::to_string(o.period)}}).memory.device;
            json.insert(1, "\"memorySetup\":{\"clocked\":" + std::string(setup.clocked ? "true" : "false") + ",\"accessCycles\":" +
                               std::to_string(setup.access_cycles) + ",\"wordsPerRow\":" + std::to_string(setup.words_per_row) +
                               ",\"refreshIntervalCycles\":" + std::to_string(setup.refresh_interval_cycles) + "},");
        }
        if (record) {
            std::ofstream stream(a.at("--recording"));
            if (!stream) throw std::runtime_error("Cannot create the recording");
            stream << "{\"schemaVersion\":1,\"kind\":\"circuitsim-circuit-recording\",\"capturePolicy\":\"presentation-checkpoints\",\"sourceRuntime\":\"native\",\"topology\":"
                   << topology << ",\"topologySlices\":" << scopes << ",\"checkpoints\":[";
            for (size_t i = 0; i < checkpoints.size(); ++i) stream << (i ? "," : "") << checkpoints[i];
            stream << "],\"recording\":" << json << "}\n";
            if (!stream) throw std::runtime_error("Cannot finish the recording");
        }
        std::cout << json << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
