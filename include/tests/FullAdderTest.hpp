#pragma once
#include "simulator/SimulationTest.hpp"

class FullAdderTest : public SimulationTest {
public:
    // We override setupCircuit to change the root component type
    void setupCircuit() override;
    std::string getTestName() const override;
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;
    size_t getRunDuration() const override;
};