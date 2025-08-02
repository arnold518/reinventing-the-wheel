#pragma once
#include "simulator/SimulationTest.hpp" // Required for the parent class

// The public interface for the FullCircuitTest.
class FullCircuitTest : public SimulationTest {
public:
    std::string getTestName() const override;
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;
    size_t getRunDuration() const override;
};