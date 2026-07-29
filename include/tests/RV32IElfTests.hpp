#pragma once

#include "tests/TestHelpers.hpp"

class RV32IElfLoaderTest : public StandaloneVerificationTest {
protected:
    std::string getTestName() const override;
    void verifyResults() override;
};
