#pragma once

#include "modules/basic/Gate.hpp"
#include "tests/ComponentRowsTest.hpp"
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

class NOTGateTest : public ComponentRowsTest<NOTGate> {
public:
    NOTGateTest();
};

class ANDGateTest : public ComponentRowsTest<ANDGate> {
public:
    ANDGateTest();
};

class ORGateTest : public ComponentRowsTest<ORGate> {
public:
    ORGateTest();
};

class XORGateTest : public ComponentRowsTest<XORGate> {
public:
    XORGateTest();
};

class NANDGateTest : public ComponentRowsTest<NANDGate> {
public:
    NANDGateTest();
};

class NORGateTest : public ComponentRowsTest<NORGate> {
public:
    NORGateTest();
};

class SRLatchTest : public SimulationTest {
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

class GatedDLatchTest : public SimulationTest {
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

class DFlipFlopTest : public SimulationTest {
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

class ClockGeneratorTest : public SimulationTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
    size_t getRunDuration() const override;

protected:
    void buildCircuit() override {}
    void setInitialState() override;
    void verifyResults() override;
};
