#pragma once

#include "tests/TestHelpers.hpp"
#include <string>

class RV32IProgramLoaderTest : public StandaloneVerificationTest {
public:
    void setupCircuit() override;
    std::string getTestName() const override;
    std::vector<SimulationCheckpoint> getCheckpoints() const override;

protected:
    void verifyResults() override;
};
