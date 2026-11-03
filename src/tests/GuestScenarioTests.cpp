#include "tests/GuestScenarioTests.hpp"
#include "tests/TestRegistry.hpp"
#include "runtime/RecordedGuestFixtures.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/FidelityPresets.hpp"
#include "system/ComputerSystem.hpp"
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
std::string hex(uint32_t value, int digits) {
    std::ostringstream text;
    text << "0x" << std::hex << std::setw(digits) << std::setfill('0') << value;
    return text.str();
}
} // namespace

SimulationTime certifiedSystemPeriod() {
    return circuit::timing::unitTiming() ? 100 : 4500;
}

GuestScenarioTest::GuestScenarioTest(GuestScenarioSpec spec)
    : spec_(std::move(spec)), profile_(circuit::fidelityPreset("compact", circuit::builtinComponentCatalog())) {
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
    auto built = circuit::builtinComponentCatalog().createRoot(
        {"system.computer", spec_.root, circuit::runtime::recordedMachineParameters(fixture.options)}, profile_);
    root = built.root;
}

void GuestScenarioTest::preparePrecomputedSimulation() {
    if (precomputed_) return;
    setupCircuit();
    auto machine = std::dynamic_pointer_cast<circuit::system::ComputerSystem>(root);
    require(machine != nullptr, "system.computer did not build a computer system");
    auto fixture = circuit::runtime::recordedGuestFixture(spec_.fixture);
    if (!spec_.machine.empty()) fixture.options.system = spec_.machine;
    circuit::runtime::retime(fixture, certifiedSystemPeriod());
    auto& o = fixture.options;
    o.capture_frames = true;
    o.on_event = [&](const GuestRunEvent& e) {
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
    };
    const auto result = circuit::runtime::runRecordedGuestOn(machine, *sim, fixture.elf, fixture.input, o);
    require(!checkpoints_.empty() && checkpoints_.back().label == "halted", "Guest scenario did not halt");
    if (spec_.expect) spec_.expect(result);
    duration_ = checkpoints_.back().time;
    cycles_ = result.cycles; instructions_ = result.instructions; frames_ = result.frames.size();
    precomputed_ = true;
}

std::map<std::string, std::string> GuestScenarioTest::unavailableFidelityPresets() const {
    // Gate cannot hold the machine's main memory. A game with the core built
    // from its stages (Logic, Visual) keeps more wire history than a browser
    // tab can hold: Pong's short version passed 3 GB natively within 100 s.
    auto result = circuit::fidelityPresetRestrictions("system.computer");
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
    for (auto& row : rows) {
        auto labels = std::vector<std::string>{"simulation", "rv32im", "device", "guest-scenario"};
        labels.insert(labels.end(), row.labels.begin(), row.labels.end());
        const auto spec = row.spec;
        // One logical test per contract: these are scenarios of ComputerSystemTest.
        entries.push_back({spec.test, "ComputerSystemTest", RegisteredTestKind::Program, {"system.computer"}, row.scenario, labels,
                           [spec] { return std::make_unique<GuestScenarioTest>(spec); }, true});
    }
}
