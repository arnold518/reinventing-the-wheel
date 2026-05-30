#pragma once

#include "simulator/SimulationTest.hpp"

class MemoryBitTest : public SimulationTest {
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

class BehavioralMemoryBitTest : public SimulationTest {
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

class BehavioralMemory64Kx32Test : public SimulationTest {
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

class Register32Test : public SimulationTest {
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

class RegisterFile32x32Test : public SimulationTest {
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

class BehavioralRegisterFile32x32Test : public RegisterFile32x32Test {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
};

class BehavioralRegisterFile32x32UnknownTest : public SimulationTest {
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
