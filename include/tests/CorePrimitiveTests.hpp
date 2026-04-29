#pragma once

#include "simulator/SimulationTest.hpp"
#include "tests/TestHelpers.hpp"
#include <string>

class WireTemplateTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class GateTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class DFlipFlopTest : public SimulationTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
    size_t getRunDuration() const override;

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
