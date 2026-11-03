#pragma once
#include "tests/TestHelpers.hpp"

// Ripple and prefix adders at 8, 16 and 32 bits: every carry span at every
// position, patterns, seeded random vectors and single unknown inputs, against
// a + b + C_IN and between the gate-level and behavioral forms.
class AdderWidthSweepTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "AdderWidthSweepTest"; }
protected:
    void verifyResults() override;
};
