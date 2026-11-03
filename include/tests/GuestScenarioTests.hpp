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
#include <string>
#include <vector>

struct TestRegistryEntry;
namespace circuit::runtime { struct RecordedGuestResult; }

struct GuestScenarioSpec {
    std::string test;          // registry name
    std::string fixture;       // tests/fixtures/rv32im/games/<fixture>.elf
    std::string machine;       // empty: the machine the fixture was built for
    std::string root;          // root instance name
    // Extra checks on the finished run (console, report, frames, LEDs).
    std::function<void(const circuit::runtime::RecordedGuestResult&)> expect;
};

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
};

// The clock period a whole computer is certified for under the active timing
// profile (§ 5.7): 100 ticks (unit) or 4.5 ns (embedded, desktop).
SimulationTime certifiedSystemPeriod();
void appendGuestScenarioTests(std::vector<TestRegistryEntry>& entries);
