#pragma once
// Scenarios that run a real guest program on a whole computer
// (`system.computer`): the short game versions and the machine scenarios of
// § 5.3. The visualizer gets the run with wire history; every instruction,
// data access, device read, frame and console byte is checked against the
// independent models of the checked runner (runtime/RecordedGuest.hpp).
#include "simulator/SimulationTest.hpp"
#include "components/selection/BuildProfile.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct TestRegistryEntry;
namespace circuit::runtime { struct RecordedGuestResult; struct GuestRunEvent; }

struct GuestScenarioSpec {
    std::string test;          // registry name
    std::string fixture;       // tests/fixtures/rv32im/games/<fixture>.elf
    std::string machine;       // empty: the machine the fixture was built for
    std::string root;          // root instance name
    // Extra checks on the finished run (console, report, frames, LEDs).
    std::function<void(const circuit::runtime::RecordedGuestResult&)> expect;
    // A replay: continue a long game's Visual recording from one of its
    // saves (circuit::state) for `cycles` cycles. Empty for a normal run.
    std::shared_ptr<const std::vector<uint8_t>> resume;
    uint64_t cycles = 0;
    bool replay = false;
};

// The save the next GameReplayScenarioTest starts from. The viewer fetches
// it from a long recording and sets it before opening the scenario.
struct GuestReplayRequest {
    std::string fixture;
    std::shared_ptr<const std::vector<uint8_t>> save;
    uint64_t cycles = 0;
};
void setGuestReplay(GuestReplayRequest request);
// The save at the end of the last replay's window (empty before one ran):
// the next window starts from it ("Continue").
std::shared_ptr<const std::vector<uint8_t>> guestReplayContinuation();

class GuestScenarioTest : public SimulationTest {
public:
    explicit GuestScenarioTest(GuestScenarioSpec spec);
    bool run() override;
    void setupCircuit() override;
    void preparePrecomputedSimulation() override;
    bool isSimulationPrecomputed() const override { return precomputed_; }
    SimulationTime getRunDuration() const override { return duration_; }
    std::string getTestName() const override { return spec_.test; }
    std::vector<SimulationCheckpoint> getCheckpoints() const override { return checkpoints_; }
    std::vector<PerformanceMetric> getPerformanceMetrics() const override;
    bool supportsBuildProfile() const override { return true; }
    std::map<std::string, std::string> unavailableFidelityPresets() const override;
    circuit::BuildProfile getBuildProfile() const override { return profile_; }
    void setBuildProfile(circuit::BuildProfile profile) override;
    // A long game's replay as a session (game-replay): go on for more cycles
    // from where it is, or jump to another frame's save on the same machine.
    // Both keep the built machine; history covers the current stretch only.
    bool isGameReplay() const noexcept { return spec_.replay; }
    void gameAdvance(uint64_t cycles);
    void gameJump(std::shared_ptr<const std::vector<uint8_t>> save);
    void gameFastForward(uint64_t cycles);
    SimulationTime gameHistoryStart() const noexcept { return history_start_; }
    uint64_t gameCycle() const noexcept { return cycles_; }
    bool gameEnded() const noexcept { return ended_; }
protected:
    void buildCircuit() override {}
    void setInitialState() override {}
    void verifyResults() override {}
private:
    GuestScenarioSpec spec_;
    circuit::BuildProfile profile_;
    std::vector<SimulationCheckpoint> checkpoints_;
    SimulationTime duration_ = 0;
    uint64_t cycles_ = 0, instructions_ = 0, frames_ = 0;
    bool precomputed_ = false;
    uint64_t resumed_cycle_ = 0;
    // A game replay session: the save where its last run stopped, the end of
    // that run and the start of the history it keeps.
    std::shared_ptr<const std::vector<uint8_t>> continuation_;
    SimulationTime session_end_ = 0, history_start_ = 0;
    bool ended_ = false;
    void noteRunEvent(const circuit::runtime::GuestRunEvent& event);
    circuit::runtime::RecordedGuestResult runFromSave(std::shared_ptr<const std::vector<uint8_t>> save, bool in_place, uint64_t cycles);
    void markRestore(const std::vector<uint8_t>& save, const char* how = "restored at");
    std::string replayPreset() const;
};

// The clock a whole computer runs its games at under the active timing
// profile (§ 5.7): 1000 ticks (unit) or 4.5 ns (embedded, desktop).
SimulationTime certifiedSystemPeriod();
void appendGuestScenarioTests(std::vector<TestRegistryEntry>& entries);
