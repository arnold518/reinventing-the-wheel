#include "runtime/RecordedGuestFixtures.hpp"
#include "simulator/MachineState.hpp"
#include <zlib.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>

// Proof that a saved machine resumes exactly (circuit::state):
//
//   MachineStateProbe FIXTURE --save CYCLE --compare CYCLE [--period TICKS]
//                     [--fidelity visual] [--out DIRECTORY]
//
// Run A goes from reset to the compare cycle and saves at both cycles. Run B
// restores A's first save into a freshly built machine and saves at the
// compare cycle. The two compare saves must be equal byte for byte (an equal
// save is an equal complete state: every wire, pin, queued event, part and
// the runner's models), and B's frames must be A's frames after the save.
// Prints each save's size raw and deflated. The timing profile is
// CIRCUITSIM_TIMING_PROFILE.
namespace {
size_t deflated(const std::vector<uint8_t>& bytes) {
    uLongf size = compressBound(bytes.size());
    std::vector<Bytef> out(size);
    if (compress2(out.data(), &size, bytes.data(), bytes.size(), 9) != Z_OK) throw std::runtime_error("deflate failed");
    return size;
}
void write(const std::filesystem::path& path, const std::vector<uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!file) throw std::runtime_error("Cannot write " + path.string());
}
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) throw std::invalid_argument("Usage: MachineStateProbe FIXTURE --save CYCLE --compare CYCLE [--period TICKS] [--fidelity visual] [--out DIRECTORY]");
        std::map<std::string, std::string> a{{"--save", "2000"}, {"--compare", "4000"}, {"--period", "4500"}, {"--fidelity", "visual"}, {"--out", ""}};
        for (int i = 2; i < argc; ++i) {
            const std::string key = argv[i];
            if (!a.contains(key) || i + 1 >= argc) throw std::invalid_argument("Unknown or incomplete option " + key);
            a[key] = argv[++i];
        }
        const uint64_t save = std::stoull(a.at("--save")), compare = std::stoull(a.at("--compare"));
        if (!save || compare <= save) throw std::invalid_argument("Needs 0 < save cycle < compare cycle");
        auto fixture = circuit::runtime::recordedGuestFixture(argv[1]);
        circuit::runtime::retime(fixture, std::stoull(a.at("--period")));
        auto options = fixture.options;
        options.fidelity_profile = a.at("--fidelity");
        options.stop_after_cycles = compare + 1;

        std::map<uint64_t, std::vector<uint8_t>> saves_a;
        std::map<uint64_t, uint64_t> frames_at;
        uint64_t frames_now = 0;
        using Content = circuit::runtime::RecordedGuestOptions::SaveContent;
        options.save_at = [&](uint64_t cycle, uint64_t presented) {
            frames_now = presented;
            return cycle == save || cycle == compare ? Content::Machine : Content::None;
        };
        options.on_save = [&](uint64_t cycle, SimulationTime, Content, std::vector<uint8_t> bytes) {
            frames_at[cycle] = frames_now;
            saves_a[cycle] = std::move(bytes);
        };
        auto started = std::chrono::steady_clock::now();
        const auto a_result = circuit::runtime::runRecordedGuest(fixture.elf, fixture.input, options);
        const double a_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        std::cerr << "run A: " << a_result.cycles << " cycles in " << a_seconds << " s\n";
        if (!saves_a.contains(save) || !saves_a.contains(compare))
            throw std::runtime_error("Run A ended before the compare cycle (it halted at cycle " + std::to_string(a_result.cycles) + ")");

        std::vector<uint8_t> save_b;
        auto resumed = fixture.options;
        resumed.fidelity_profile = options.fidelity_profile;
        // B stops at the compare cycle: its save there is the one taken when a
        // run stops (a replay's end), compared with A's save at the top of it.
        resumed.stop_after_cycles = compare;
        resumed.resume = std::make_shared<const std::vector<uint8_t>>(saves_a.at(save));
        resumed.save_at = [&](uint64_t cycle, uint64_t) { return cycle == compare ? Content::Machine : Content::None; };
        resumed.on_save = [&](uint64_t, SimulationTime, Content, std::vector<uint8_t> bytes) { save_b = std::move(bytes); };
        started = std::chrono::steady_clock::now();
        const auto b_result = circuit::runtime::runRecordedGuest(fixture.elf, fixture.input, resumed);
        const double b_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

        const auto& save_a = saves_a.at(compare);
        if (save_b != save_a) {
            size_t first = 0;
            while (first < save_a.size() && first < save_b.size() && save_a[first] == save_b[first]) ++first;
            throw std::runtime_error("FAIL: the resumed save differs from the uninterrupted one at byte " + std::to_string(first) +
                                     " of " + std::to_string(save_a.size()) + "/" + std::to_string(save_b.size()));
        }
        const size_t skipped = frames_at.at(save);
        size_t a_frames = 0;
        for (const auto cycle : a_result.frame_cycles) a_frames += cycle <= compare;
        if (b_result.frames.size() + skipped != a_frames) throw std::runtime_error("FAIL: frame counts differ");
        for (size_t i = 0; i < b_result.frames.size(); ++i) {
            const auto& x = a_result.frames[skipped + i];
            const auto& y = b_result.frames[i];
            if (x.time != y.time || x.sequence != y.sequence || x.pixels != y.pixels || x.known != y.known)
                throw std::runtime_error("FAIL: frame " + std::to_string(skipped + i) + " differs after resuming");
        }
        if (!a.at("--out").empty()) {
            const std::filesystem::path out = a.at("--out");
            std::filesystem::create_directories(out);
            for (const auto& [cycle, bytes] : saves_a) write(out / ("save-" + std::to_string(cycle) + ".bin"), bytes);
        }
        std::cout << "PASS " << fixture.name << " (" << options.fidelity_profile << "): resumed at cycle " << save
                  << " and matched cycle " << compare << " byte for byte; " << b_result.frames.size() << " frames after the save equal\n";
        for (const auto& [cycle, bytes] : saves_a)
            std::cout << "  save at cycle " << cycle << ": " << bytes.size() << " bytes, " << deflated(bytes) << " deflated\n";
        std::cout << "  run A " << a_seconds << " s (" << compare + 1 << " cycles), run B " << b_seconds << " s ("
                  << compare - save << " cycles)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
