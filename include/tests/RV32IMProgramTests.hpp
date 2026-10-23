#pragma once

#include "simulator/SimulationTest.hpp"
#include "components/selection/BuildProfile.hpp"
#include <map>

class RV32IProgramRoot;
class IOComponent;
class WireBase;
struct TestRegistryEntry;

// Short, recorded hardware programs. The independent instruction oracle only
// checks retirement; real instruction/data endpoints drive the core.
class RV32IMProgramTest : public SimulationTest {
public:
    explicit RV32IMProgramTest(size_t program);
    bool run() override;
    void setupCircuit() override;
    void preparePrecomputedSimulation() override;
    bool isSimulationPrecomputed() const override { return precomputed_; }
    SimulationTime getRunDuration() const override { return duration_; }
    std::string getTestName() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override { return checkpoints_; }
    std::vector<PerformanceMetric> getPerformanceMetrics() const override;
    bool supportsBuildProfile() const override { return true; }
    circuit::BuildProfile getBuildProfile() const override { return profile_; }
    void setBuildProfile(circuit::BuildProfile profile) override;
protected:
    void buildCircuit() override {}
    void setInitialState() override {}
    void verifyResults() override {}
private:
    size_t program_;
    circuit::BuildProfile profile_;
    std::shared_ptr<RV32IProgramRoot> fixture_;
    std::shared_ptr<IOComponent> core_;
    std::map<std::string,std::shared_ptr<WireBase>> inputs_;
    std::vector<SimulationCheckpoint> checkpoints_;
    SimulationTime duration_=0,period_=200;
    size_t commits_=0,cycles_=0,arithmetic_stalls_=0;
    bool precomputed_=false;
};

void appendRV32IMProgramTests(std::vector<TestRegistryEntry>& entries);
