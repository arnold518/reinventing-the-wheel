#include "tests/GuestScenarioTests.hpp"
#include "tests/TestRegistry.hpp"
#include "runtime/RecordedGuestFixtures.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/FidelityPresets.hpp"
#include "system/ComputerSystem.hpp"
#include "simulator/StateArchive.hpp"
#include "simulator/MachineState.hpp"
#include "runtime/RecordedGuest.hpp"
#include <mutex>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <iomanip>
#include <iostream>
#include <sstream>

using circuit::runtime::GuestRunEvent;
using circuit::runtime::RecordedGuestResult;

namespace {
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
uint32_t word(const RecordedGuestResult& r, size_t index) {
    uint32_t value = 0;
    for (unsigned byte = 0; byte < 4; ++byte) value |= uint32_t(r.report.at(4 * index + byte)) << (8 * byte);
    return value;
}
const char* buttonName(unsigned button) {
    static const char* names[] = {"UP", "RIGHT", "DOWN", "LEFT", "ACTION", "RESTART", "UPDATE", "P2 UP", "P2 DOWN", "PAUSE"};
    return button < 10 ? names[button] : "button";
}
GuestReplayRequest& replayRequest() {
    static GuestReplayRequest request;
    return request;
}
std::mutex& replayMutex() {
    static std::mutex mutex;
    return mutex;
}
std::shared_ptr<const std::vector<uint8_t>>& replayContinuation() {
    static std::shared_ptr<const std::vector<uint8_t>> save;
    return save;
}
// The cycle a run save starts at (its header: tag, version, identity, cycle).
uint64_t savedCycle(const std::vector<uint8_t>& save) { return circuit::runtime::savedRunCycle(save); }
// The simulator time a run save holds (the restored machine's present).
SimulationTime savedTime(const std::vector<uint8_t>& save) {
    const auto machine = circuit::runtime::savedMachine(save);
    circuit::state::Reader m(machine);
    m.u32(); m.u32(); m.u64(); m.u64(); m.u64(); m.u64(); m.u32();
    return m.time();
}
std::string hex(uint32_t value, int digits) {
    std::ostringstream text;
    text << "0x" << std::hex << std::setw(digits) << std::setfill('0') << value;
    return text.str();
}
} // namespace

void setGuestReplay(GuestReplayRequest request) {
    const std::lock_guard<std::mutex> lock(replayMutex());
    replayRequest() = std::move(request);
}

std::shared_ptr<const std::vector<uint8_t>> guestReplayContinuation() {
    const std::lock_guard<std::mutex> lock(replayMutex());
    return replayContinuation();
}

// The named preset of the session's build profile ("fidelity-<preset>-v1").
std::string GuestScenarioTest::replayPreset() const {
    const std::string name = profile_.name(), prefix = "fidelity-", suffix = "-v1";
    if (name.rfind(prefix, 0) == 0 && name.size() > prefix.size() + suffix.size() &&
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
        return name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
    return "visual";
}

// The games' clock, which the SRAM main memory is set up for and the clock
// certificate covers (ComputerSystemTimingTest): 1000 ticks in unit, 4.5 ns
// otherwise.
SimulationTime certifiedSystemPeriod() {
    return circuit::timing::unitTiming() ? 1000 : 4500;
}

GuestScenarioTest::GuestScenarioTest(GuestScenarioSpec spec)
    : spec_(std::move(spec)),
      profile_(circuit::fidelityPreset(spec_.replay ? "visual" : "compact", circuit::builtinComponentCatalog())) {
    (void)circuit::runtime::recordedGuestFixture(spec_.fixture);
}

void GuestScenarioTest::setBuildProfile(circuit::BuildProfile profile) {
    profile_ = std::move(profile);
    root.reset(); builder.reset(); sim = std::make_shared<Simulator>();
    checkpoints_.clear(); initial_events_scheduled_ = precomputed_ = false;
    duration_ = 0; cycles_ = instructions_ = frames_ = 0;
}

void GuestScenarioTest::setupCircuit() {
    if (root) return;
    auto fixture = circuit::runtime::recordedGuestFixture(spec_.fixture);
    if (!spec_.machine.empty()) fixture.options.system = spec_.machine;
    // The memory is set up for the clock the scenario runs at.
    circuit::runtime::retime(fixture, certifiedSystemPeriod());
    auto built = circuit::builtinComponentCatalog().createRoot(
        {"system.computer", spec_.root, circuit::runtime::recordedMachineParameters(fixture.options)}, profile_);
    root = built.root;
}

// Timeline marks for the checked runner's events: reset, presses, console
// lines, frames and the halt.
void GuestScenarioTest::noteRunEvent(const GuestRunEvent& e) {
    std::string label, detail;
    const auto where = "cycle " + std::to_string(e.cycle) + ", " + std::to_string(e.instructions) + " instructions retired";
    switch (e.kind) {
    case GuestRunEvent::Kind::ResetReleased:
        label = "reset released";
        detail = "RST is low; the core fetches its first instruction from address 0x00000000.";
        break;
    case GuestRunEvent::Kind::InputSampled: {
        const unsigned button = e.value & 31u;
        const bool down = (e.value >> 8) & 1u;
        label = std::string(down ? "press " : "release ") + buttonName(button);
        detail = "Button " + std::to_string(button) + (down ? " down" : " up") +
                 " enters the button queue; the program reads it from BUTTONS_HEAD (" + where + ").";
        break;
    }
    case GuestRunEvent::Kind::ConsoleLine:
        label = "console: " + e.text;
        detail = "The console has sent \"" + e.text + "\\n\" to the host (" + where + ").";
        break;
    case GuestRunEvent::Kind::Presented:
        label = "frame " + std::to_string(e.value);
        detail = "PRESENT number " + std::to_string(e.value) + ": the display shows video memory as written (" + where + ")";
        if (!e.text.empty()) detail += "; LED pins " + hex(uint32_t(std::stoul(e.text)), 4);
        detail += ".";
        break;
    case GuestRunEvent::Kind::Halted:
        label = "halted";
        detail = "EBREAK after " + std::to_string(e.cycle + 1) + " cycles and " + std::to_string(e.instructions) +
                 " instructions. Every instruction, data access, device read, frame and console byte matched the independent models.";
        break;
    }
    checkpoints_.push_back({e.time, label, detail, checkpoints_.size()});
}

// The checked runner on this machine from a save: a full restore (opening
// or jumping to a frame) or, in place, from where the last run stopped. It
// stops `cycles` later and saves there, so the session can go on.
circuit::runtime::RecordedGuestResult GuestScenarioTest::runFromSave(std::shared_ptr<const std::vector<uint8_t>> save,
                                                                     bool in_place, uint64_t cycles) {
    // Restored without running: the session continues from that very save.
    // (A version 1 save is converted by saving again: only version 2 can
    // continue in place.)
    const bool restore_only = cycles == 0 && !in_place && circuit::runtime::savedRunVersion(*save) >= 2;
    auto machine = std::dynamic_pointer_cast<circuit::system::ComputerSystem>(root);
    require(machine != nullptr, "system.computer did not build a computer system");
    auto fixture = circuit::runtime::recordedGuestFixture(spec_.fixture);
    circuit::runtime::retime(fixture, certifiedSystemPeriod());
    auto& o = fixture.options;
    o.capture_frames = true;
    // The save names the program, input and options it was made with
    // (the fidelity preset too), and the machine it holds; a mismatch is
    // refused, not hidden. The session's preset is the recording's.
    o.fidelity_profile = replayPreset();
    o.resume = save;
    o.resume_in_place = in_place;
    o.stop_after_cycles = savedCycle(*save) + cycles;
    using Content = circuit::runtime::RecordedGuestOptions::SaveContent;
    const uint64_t stop = o.stop_after_cycles;
    if (!restore_only) {
        o.save_at = [stop](uint64_t cycle, uint64_t) { return cycle == stop ? Content::Machine : Content::None; };
        o.on_save = [this](uint64_t, SimulationTime, Content, std::vector<uint8_t> bytes) {
            continuation_ = std::make_shared<const std::vector<uint8_t>>(std::move(bytes));
            const std::lock_guard<std::mutex> lock(replayMutex());
            replayContinuation() = continuation_;
        };
    }
    o.on_event = [this](const GuestRunEvent& e) { noteRunEvent(e); };
    auto result = circuit::runtime::runRecordedGuestOn(machine, *sim, fixture.elf, fixture.input, o);
    if (restore_only) {
        continuation_ = save;
        const std::lock_guard<std::mutex> lock(replayMutex());
        replayContinuation() = continuation_;
    }
    // The program halted inside this run: the session has reached the end.
    ended_ = !result.stopped;
    session_end_ = o.origin + o.period * result.cycles - 1;
    cycles_ = result.cycles; instructions_ = result.instructions; frames_ = result.frames.size();
    return result;
}

void GuestScenarioTest::markRestore(const std::vector<uint8_t>& save, const char* how) {
    const auto cycle = savedCycle(save);
    resumed_cycle_ = cycle;
    history_start_ = savedTime(save);
    auto fixture = circuit::runtime::recordedGuestFixture(spec_.fixture);
    circuit::runtime::retime(fixture, certifiedSystemPeriod());
    // The last tick of the cycle before the save: the state the save holds.
    checkpoints_.push_back({fixture.options.origin + fixture.options.period * cycle - 1,
        std::string(how) + " cycle " + std::to_string(cycle),
        std::string(how) == "restored at"
            ? "The whole machine as the recording saved it at cycle " + std::to_string(cycle) +
              ": every wire, pin, part and memory byte. From here it runs again, event by event."
            : "The machine simulated up to cycle " + std::to_string(cycle) +
              " without keeping its events; from here every event is kept.", checkpoints_.size()});
}

void GuestScenarioTest::preparePrecomputedSimulation() {
    if (precomputed_) return;
    require(!spec_.replay || spec_.resume, "A game replay opens from a long recording");
    setupCircuit();
    if (spec_.replay) {
        markRestore(*spec_.resume);
        const auto result = runFromSave(spec_.resume, false, spec_.cycles);
        if (spec_.cycles)
            checkpoints_.push_back({session_end_, "end of the replay",
                "Cycle " + std::to_string(result.cycles) + ": the replay's window ends. Every instruction, data access and "
                "device read since the save matched the independent models.", checkpoints_.size()});
        duration_ = std::max(session_end_, checkpoints_.back().time);
        precomputed_ = true;
        return;
    }
    auto machine = std::dynamic_pointer_cast<circuit::system::ComputerSystem>(root);
    require(machine != nullptr, "system.computer did not build a computer system");
    auto fixture = circuit::runtime::recordedGuestFixture(spec_.fixture);
    if (!spec_.machine.empty()) fixture.options.system = spec_.machine;
    circuit::runtime::retime(fixture, certifiedSystemPeriod());
    auto& o = fixture.options;
    o.capture_frames = true;
    o.on_event = [this](const GuestRunEvent& e) { noteRunEvent(e); };
    const auto result = circuit::runtime::runRecordedGuestOn(machine, *sim, fixture.elf, fixture.input, o);
    require(!checkpoints_.empty() && checkpoints_.back().label == "halted", "Guest scenario did not halt");
    if (spec_.expect) spec_.expect(result);
    duration_ = checkpoints_.back().time;
    cycles_ = result.cycles; instructions_ = result.instructions; frames_ = result.frames.size();
    precomputed_ = true;
}

void GuestScenarioTest::gameAdvance(uint64_t cycles) {
    require(spec_.replay && precomputed_ && continuation_, "Only an open game replay can continue");
    if (ended_) return;  // the program has halted; nothing follows
    require(cycles > 0 && cycles <= 100000, "A game replay continues by 1 to 100000 cycles");
    // Viewing history moved the wire and pin values; the save made where the
    // last run stopped puts them back before the run goes on.
    {
        const auto machine = circuit::runtime::savedMachine(*continuation_);
        circuit::state::Reader values(machine);
        circuit::state::restoreValues(root, *sim, values);
    }
    runFromSave(continuation_, true, cycles);
    duration_ = session_end_;
    // A session keeps the last stretch of history only (each cycle of the
    // Visual machine records about 360 KB): past 1.5 windows, the history
    // before the last window is dropped, with the checkpoints in it.
    const auto window = std::getenv("CIRCUITSIM_GAME_HISTORY_CYCLES")
        ? std::stoull(std::getenv("CIRCUITSIM_GAME_HISTORY_CYCLES")) : uint64_t{600};
    auto fixture = circuit::runtime::recordedGuestFixture(spec_.fixture);
    circuit::runtime::retime(fixture, certifiedSystemPeriod());
    const auto span = static_cast<SimulationTime>(window) * fixture.options.period;
    if (session_end_ > history_start_ && session_end_ - history_start_ > span + span / 2) {
        history_start_ = session_end_ - span;
        sim->trimHistoryBefore(history_start_);
        std::erase_if(checkpoints_, [&](const auto& checkpoint) { return checkpoint.time < history_start_; });
        for (size_t i = 0; i < checkpoints_.size(); ++i) checkpoints_[i].row_index = i;
    }
}

// A seek ahead: the cycles run without history (about twice as fast), then
// the history starts again where they stopped.
void GuestScenarioTest::gameFastForward(uint64_t cycles) {
    require(spec_.replay && precomputed_ && continuation_, "Only an open game replay can continue");
    require(cycles > 0 && cycles <= 100000, "A game replay continues by 1 to 100000 cycles");
    if (ended_) return;
    {
        const auto machine = circuit::runtime::savedMachine(*continuation_);
        circuit::state::Reader values(machine);
        circuit::state::restoreValues(root, *sim, values);
    }
    sim->setHistoryRecordingEnabled(false);
    runFromSave(continuation_, true, cycles);
    sim->setHistoryRecordingEnabled(true);
    require(!ended_, "The program ended while seeking");
    const auto at = continuation_;
    checkpoints_.clear();
    markRestore(*at, "simulated to");
    runFromSave(at, false, 0);
    duration_ = checkpoints_.back().time;
}

void GuestScenarioTest::gameJump(std::shared_ptr<const std::vector<uint8_t>> save) {
    require(spec_.replay && precomputed_ && save, "Only an open game replay can jump to a save");
    checkpoints_.clear();
    markRestore(*save);
    runFromSave(save, false, 0);
    duration_ = checkpoints_.back().time;
}

std::map<std::string, std::string> GuestScenarioTest::unavailableFidelityPresets() const {
    // Gate cannot hold the machine's main memory. A game with the core built
    // from its stages (Logic, Visual) keeps more wire history than a browser
    // tab can hold: Pong's short version passed 3 GB natively within 100 s.
    auto result = circuit::fidelityPresetRestrictions("system.computer");
    if (spec_.replay) {
        // A recording's saves hold the machine it was recorded with: Visual
        // or Compact (the viewer opens the one the recording names).
        for (const auto* preset : {"gate", "logic"})
            result[preset] = "A long game's recordings are made in Visual and Compact; their saves continue only in those.";
        return result;
    }
    if (spec_.fixture != "bringup" && spec_.fixture != "bringup-leds") {
        result["gate"] = "Gate is unavailable for a computer system: its main memory exceeds the structural "
                         "storage limit. A game opens in Compact.";
        for (const auto* preset : {"logic", "visual"})
            result[preset] = "With the core built from its stages, a game keeps more than 3 GB of wire history, "
                             "more than a browser tab can hold. To watch the core's stages work, open the "
                             "machine scenario (computer-system) in Visual.";
    }
    return result;
}

bool GuestScenarioTest::run() {
    try {
        setupCircuit();
        preparePrecomputedSimulation();
        std::cout << "[PASS] " << getTestName() << ": " << spec_.fixture << ", " << cycles_ << " cycles, "
                  << instructions_ << " instructions, " << frames_ << " frames\n";
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[FAIL] " << getTestName() << ": " << e.what() << '\n';
        return false;
    }
}

std::vector<SimulationTest::PerformanceMetric> GuestScenarioTest::getPerformanceMetrics() const {
    auto metrics = SimulationTest::getPerformanceMetrics();
    metrics.insert(metrics.begin(), {
        {"guest.cycles", double(cycles_), "cycles", "Clock cycles from reset to EBREAK."},
        {"guest.instructions", double(instructions_), "instructions", "Instructions checked against the RV32IM model."},
        {"guest.frames", double(frames_), "frames", "Frames presented and checked."}});
    return metrics;
}

void appendGuestScenarioTests(std::vector<TestRegistryEntry>& entries) {
    using Result = RecordedGuestResult;
    const auto game = [](std::string console, size_t frames, uint32_t score, uint32_t status) {
        return [=](const Result& r) {
            require(r.console == console, "Console output differs: " + r.console);
            require(r.frames.size() == frames, "Frame count differs");
            require(word(r, 0) == 0x4353474d && word(r, 1) == 1 && word(r, 8) == 0, "Game report or lost input");
            require(word(r, 4) == score && word(r, 5) == status, "Score or final state differs");
        };
    };
    const auto bringup = [](const Result& r) {
        require(r.console == "hello-device\n" && r.frames.size() == 2, "Bring-up console or frames differ");
        require(word(r, 3) == 0x101 && word(r, 4) == 2, "Bring-up report differs");
    };
    struct Row { GuestScenarioSpec spec; std::string scenario; std::vector<std::string> labels; };
    std::vector<Row> rows{
        // The whole computer of § 5.3 on each machine: the bring-up program.
        {{"ComputerSystemScenarioTest", "bringup", "games-machine", "COMPUTER_SYSTEM_ROOT", bringup}, "bring-up", {"system-scenario"}},
        {{"ComputerSystemTwoBusScenarioTest", "bringup", "games-machine-two-bus", "TWO_BUS_SYSTEM_ROOT", bringup}, "bring-up", {"system-scenario"}},
        {{"ComputerSystemRegisteredBridgeScenarioTest", "bringup", "games-machine-registered-bridge", "REGISTERED_BRIDGE_SYSTEM_ROOT", bringup}, "bring-up", {"system-scenario"}},
        {{"ComputerSystemCheckedScenarioTest", "bringup", "games-machine-checked", "CHECKED_SYSTEM_ROOT", bringup}, "bring-up", {"system-scenario"}},
        {{"ComputerSystemLedsScenarioTest", "bringup-leds", "", "LED_SYSTEM_ROOT", [bringup](const Result& r) {
            bringup(r);
            require(r.frame_leds.size() == 2 && r.frame_leds[0] == 0xff, "The low eight LEDs are lit in frame 1");
        }}, "bring-up", {"system-scenario"}},
        // The short game versions (§ 5.6): a prepared board, the key event.
        {{"Game2048ScenarioTest", "2048-short", "", "GAME_MACHINE", game("2048\ndone\n", 2, 8, 0)}, "game-2048", {"games"}},
        {{"GameSokobanScenarioTest", "sokoban-short", "", "GAME_MACHINE", game("sokoban\ndone\n", 2, 1, 1)}, "game-sokoban", {"games"}},
        {{"GameSnakeScenarioTest", "snake-short", "", "GAME_MACHINE", game("snake\ndone\n", 3, 1, 0)}, "game-snake", {"games"}},
        {{"GamePongScenarioTest", "pong-short", "", "GAME_MACHINE", game("pong\ndone\n", 3, 0, 0)}, "game-pong", {"games"}},
        {{"GameTetrisScenarioTest", "tetris-short", "", "GAME_MACHINE", game("tetris\ndone\n", 2, 100, 0)}, "game-tetris", {"games"}},
        {{"GameSnakeLedsScenarioTest", "snake-leds-short", "", "GAME_MACHINE", [game](const Result& r) {
            game("snake\ndone\n", 3, 0, 2)(r);
            require(r.frame_leds.size() == 3 && r.frame_leds[0] == 0x7 && r.frame_leds[1] == 0x7,
                    "Three LEDs are lit while the snake is three long");
        }}, "game-snake-leds", {"games"}},
    };
    // A long game's replay from a save set by setGuestReplay (the viewer's
    // "Step through events"). Without one it cannot run.
    entries.push_back({"GameReplayScenarioTest", "ComputerSystemTest", RegisteredTestKind::Program, {"system.computer"}, "game-replay",
                       {"simulation", "rv32im", "device", "guest-scenario", "games", "replay"},
                       [] {
                           GuestReplayRequest request;
                           {
                               const std::lock_guard<std::mutex> lock(replayMutex());
                               request = replayRequest();
                           }
                           // Natively (SimulationTestRunner, checks): the save file, its
                           // long game and the cycles from the environment.
                           if (!request.save)
                               if (const char* file = std::getenv("CIRCUITSIM_REPLAY_SAVE")) {
                                   std::ifstream in(file, std::ios::binary);
                                   std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                                   require(!bytes.empty(), std::string("Cannot read the replay save ") + file);
                                   const char* fixture = std::getenv("CIRCUITSIM_REPLAY_FIXTURE");
                                   const char* cycles = std::getenv("CIRCUITSIM_REPLAY_CYCLES");
                                   request = {fixture ? fixture : "", std::make_shared<const std::vector<uint8_t>>(std::move(bytes)),
                                              cycles ? std::stoull(cycles) : 0};
                               }
                           // The root has the recording's name: the save names every part.
                           GuestScenarioSpec spec{"GameReplayScenarioTest", request.save ? request.fixture : "pong-long", "", "MACHINE", {}};
                           spec.resume = request.save;
                           spec.cycles = request.cycles;
                           spec.replay = true;
                           return std::make_unique<GuestScenarioTest>(spec);
                       }, true});
    for (auto& row : rows) {
        auto labels = std::vector<std::string>{"simulation", "rv32im", "device", "guest-scenario"};
        labels.insert(labels.end(), row.labels.begin(), row.labels.end());
        const auto spec = row.spec;
        // One logical test per contract: these are scenarios of ComputerSystemTest.
        entries.push_back({spec.test, "ComputerSystemTest", RegisteredTestKind::Program, {"system.computer"}, row.scenario, labels,
                           [spec] { return std::make_unique<GuestScenarioTest>(spec); }, true});
    }
}
