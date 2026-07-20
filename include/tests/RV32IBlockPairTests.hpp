#pragma once

#include "simulator/SimulationTest.hpp"

class RV32IControlFlowUnitPairTest : public SimulationTest {
public:
    std::string getTestName() const override;
    void setupCircuit() override;
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;
};

class RV32IDecodeControlUnitPairTest : public SimulationTest {
public:
    std::string getTestName() const override;
    void setupCircuit() override;
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;
};

class RV32IRegisterFilePairTest : public SimulationTest {
public:
    std::string getTestName() const override;
    void setupCircuit() override;
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;
};

class BehavioralALU32PairTest : public SimulationTest {
public:
    std::string getTestName() const override;
    void setupCircuit() override;
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;
};

class RV32IExecutionControlStatusUnitPairTest : public SimulationTest {
public:
    std::string getTestName() const override;
    void setupCircuit() override;
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;
    size_t getRunDuration() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;
};
