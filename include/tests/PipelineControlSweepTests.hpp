#pragma once
#include "tests/TestHelpers.hpp"

// Enumerates the RV32I forwarding and load-use hazard units against small
// independent rules, on the gate-level and behavioral forms.
class RV32IPipelineControlSweepTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "RV32IPipelineControlSweepTest"; }
protected:
    void verifyResults() override;
};
