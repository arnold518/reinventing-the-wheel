#pragma once

#include "tests/ComponentTestModel.hpp"
#include <map>

class WireBase;

class CacheReadTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "CacheReadTest"; }
protected:
    void verifyResults() override;
};

class CacheComponentScenarioTest : public circuit::test::ComponentScenarioTest {
public:
    CacheComponentScenarioTest();
};

class CacheDirectMappedScenarioTest : public circuit::test::ComponentScenarioTest {
public:
    CacheDirectMappedScenarioTest();
};

class CacheScalableScenarioTest : public circuit::test::ComponentScenarioTest {
public:
    CacheScalableScenarioTest();
};

class CacheTwoLevelScenarioTest : public SimulationTest {
public:
    std::string getTestName() const override { return "CacheTwoLevelScenarioTest"; }
    void setupCircuit() override;
    void preparePrecomputedSimulation() override;
    bool run() override;
    bool isSimulationPrecomputed() const override { return true; }
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;

protected:
    void buildCircuit() override {}
    void setInitialState() override {}
    void verifyResults() override {}

private:
    std::vector<SimulationCheckpoint> checkpoints_;
    std::map<std::string, std::shared_ptr<WireBase>> inputs_;
    bool prepared_ = false;
};
