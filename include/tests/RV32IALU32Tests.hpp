#pragma once

#include "tests/TestHelpers.hpp"
#include <string>

class RV32IALU32Test : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};
