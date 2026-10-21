#pragma once

#include "simulator/SimulationTime.hpp"
#include "simulator/SimulationTest.hpp"

class FullCircuitTest : public SimulationTest {
public:
    // We override setupCircuit to change the root component type
    void setupCircuit() override;
    std::string getTestName() const override;
    void buildCircuit() override;
    void setInitialState() override;
    void verifyResults() override;
    SimulationTime getRunDuration() const override;
};