#pragma once

#include "simulator/SimulationTest.hpp"

class MemoryBitStructuralContractTest : public SimulationTest {
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

class MemoryBitBehavioralContractTest : public SimulationTest {
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

class Memory64Kx32Test : public SimulationTest {
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

class Register32StructuralContractTest : public SimulationTest {
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

class Register32CellArrayContractTest : public Register32StructuralContractTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
};

class Register32BehavioralContractTest : public Register32StructuralContractTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
};

class RegisterFile4x32Test : public SimulationTest {
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

class RegisterFile32x32StructuralContractTest : public SimulationTest {
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

class RegisterFile32x32BehavioralContractTest : public RegisterFile32x32StructuralContractTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
};

class RegisterFile32x32BehavioralUnknownPolicyTest : public SimulationTest {
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

class Memory4x32Test : public SimulationTest {
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

class Memory32x32Test : public SimulationTest {
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
