#pragma once

#include "tests/TestHelpers.hpp"
#include <string>

class RV32IInstructionOracleTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};
