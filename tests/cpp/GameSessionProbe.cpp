#include "tests/GuestScenarioTests.hpp"
#include "visualizer/VisualizerSession.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

// A game replay as a session (the recording viewer's stepping): opened at a
// save, it simulates more cycles in steps, the history between steps viewed
// (which moves the wire values), and jumps back to a save. The machine at the
// end must be the one a single uninterrupted run reaches: its save equal byte
// for byte.
//
//   GameSessionProbe FIXTURE SAVE-FILE [PRESET]   (CIRCUITSIM_TIMING_PROFILE as recorded; PRESET visual or compact)
namespace {
std::shared_ptr<const std::vector<uint8_t>> read(const char* path) {
    std::ifstream in(path, std::ios::binary);
    auto bytes = std::make_shared<std::vector<uint8_t>>(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (bytes->empty()) throw std::runtime_error(std::string("Cannot read ") + path);
    return bytes;
}
// A save without what only observes the run: the simulator's activity
// counters and the last values of wire-less outputs kept while history is
// off. A seek runs part of the way without history, which changes those and
// nothing that decides an event.
std::vector<uint8_t> circuitOnly(const std::vector<uint8_t>& save) {
    auto le = [](const std::vector<uint8_t>& b, size_t at, int n) { uint64_t v = 0; for (int i = 0; i < n; ++i) v |= uint64_t(b[at + i]) << (8 * i); return v; };
    const size_t machine = 4 + 4 + 8 + 8 + 8;            // run header, then the machine's length
    std::vector<uint8_t> out(save.begin(), save.begin() + machine - 8);
    out.insert(out.end(), 8, 0);                          // the machine's length follows the pin record
    size_t at = machine;
    const size_t queue = at + 40 + 4 + 8 + 8;             // machine header, queue tag, time, next order
    std::vector<uint8_t> m(save.begin() + at, save.begin() + queue);
    m.insert(m.end(), 72, 0);                             // the counters, zeroed
    at = queue + 72;
    const auto events = le(save, at, 8);
    size_t end = at + 8;
    for (uint64_t i = 0; i < events; ++i) end += 8 + 8 + 1 + (save[end + 16] == 0 ? 4 + 16 : 4);
    m.insert(m.end(), save.begin() + at, save.begin() + end);
    const auto headless = le(save, end, 8);
    size_t after = end + 8;
    for (uint64_t i = 0; i < headless; ++i) after += 4 + 8 + 8 + le(save, after + 12, 8);
    m.insert(m.end(), 8, 0);                              // no headless pin values
    m.insert(m.end(), save.begin() + after, save.end());
    out.insert(out.end(), m.begin(), m.end());
    return out;
}
size_t count(const std::string& json) {
    const auto at = json.find("\"timestampCount\":");
    return std::stoull(json.substr(at + 17));
}
std::string preset = "visual";
std::shared_ptr<const std::vector<uint8_t>> session(const std::string& fixture, std::shared_ptr<const std::vector<uint8_t>> save,
                                                    uint64_t cycles, const std::vector<uint64_t>& steps, bool jump_back) {
    setGuestReplay({fixture, save, cycles});
    circuit::visualizer::VisualizerSession view("GameReplayScenarioTest", "game-replay", {}, 0, preset);
    for (const auto step : steps) {
        const auto timeline = view.gameAdvanceJson(step);
        const auto n = count(timeline);
        // View the history like a reader would: early, middle, then late.
        for (const size_t index : {size_t{0}, n / 3, n / 2, n - 1}) view.stateJson(index);
        std::cout << "  +" << step << " cycles: " << n << " timestamps\n";
    }
    if (jump_back) {
        view.gameJumpJson(save);
        view.stateJson(0);
        view.gameAdvanceJson(300);
    }
    return guestReplayContinuation();
}
}

int main(int argc, char** argv) {
    try {
        if (argc != 3 && argc != 4) throw std::invalid_argument("Usage: GameSessionProbe FIXTURE SAVE-FILE [PRESET]");
        if (argc == 4) preset = argv[3];
        const std::string fixture = argv[1];
        const auto save = read(argv[2]);
        std::cout << "single run of 300 cycles\n";
        const auto single = session(fixture, save, 300, {}, false);
        std::cout << "session: opened, then 100 + 50 + 150 cycles with history viewed between\n";
        const auto stepped = session(fixture, save, 0, {100, 50, 150}, false);
        if (!single || !stepped || *single != *stepped) throw std::runtime_error("FAIL: the stepped session ends in another state");
        std::cout << "session: 120 cycles, jump back to the save, then 300 cycles\n";
        const auto jumped = session(fixture, save, 0, {120}, true);
        if (!jumped || *jumped != *single) throw std::runtime_error("FAIL: after a jump the session ends in another state");
        std::cout << "session: 150 cycles without history (a seek), then 150 with history\n";
        setGuestReplay({fixture, save, 0});
        {
            circuit::visualizer::VisualizerSession view("GameReplayScenarioTest", "game-replay", {}, 0, preset);
            view.gameFastForwardJson(150);
            const auto timeline = view.gameAdvanceJson(150);
            view.stateJson(count(timeline) / 2);
        }
        const auto seeked = guestReplayContinuation();
        if (const char* dump = std::getenv("GAME_SESSION_DUMP")) {
            std::ofstream(std::string(dump) + "-single.bin", std::ios::binary).write(reinterpret_cast<const char*>(single->data()), single->size());
            std::ofstream(std::string(dump) + "-seeked.bin", std::ios::binary).write(reinterpret_cast<const char*>(seeked->data()), seeked->size());
        }
        if (!seeked || circuitOnly(*seeked) != circuitOnly(*single))
            throw std::runtime_error("FAIL: after a seek the session ends in another state");
        std::cout << "  equal except the activity counters and history-off pin record: "
                  << (*seeked == *single ? "fully equal" : "those differ only") << "\n";
        std::cout << "PASS " << fixture << ": stepped, viewed, jumped and seeked sessions end in the single run's state ("
                  << single->size() << " bytes, equal)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
