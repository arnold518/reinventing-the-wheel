#pragma once

#include "modules/basic/Gate.hpp"
#include "tests/TruthTableComponentTest.hpp"
#include "simulator/SimulationTest.hpp"
#include "tests/TestHelpers.hpp"
#include <string>

class WireTemplateTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class SimulatorAdvanceAndRecordTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class SimulatorUnwiredOutputPinHistoryTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class SimulatorDrainUntilIdleTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class NOTGateTest : public TruthTableComponentTest<NOTGate> {
public:
    NOTGateTest();
};

class ANDGateTest : public TruthTableComponentTest<ANDGate> {
public:
    ANDGateTest();
};

class ORGateTest : public TruthTableComponentTest<ORGate> {
public:
    ORGateTest();
};

class XORGateTest : public TruthTableComponentTest<XORGate> {
public:
    XORGateTest();
};

class NANDGateTest : public TruthTableComponentTest<NANDGate> {
public:
    NANDGateTest();
};

class NORGateTest : public TruthTableComponentTest<NORGate> {
public:
    NORGateTest();
};

class SRLatchTest : public circuit::test::ComponentScenarioTest {
public:
    SRLatchTest();
};

class GatedDLatchTest : public circuit::test::ComponentScenarioTest {
public:
    GatedDLatchTest();
};

class DFlipFlopTest : public circuit::test::ComponentScenarioTest {
public:
    DFlipFlopTest();
};

class ClockGeneratorTest : public SimulationTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;

protected:
    void buildCircuit() override {}
    void setInitialState() override;
    void verifyResults() override;
};
