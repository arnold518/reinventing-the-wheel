#pragma once

#include "tests/TestHelpers.hpp"
#include <string>

class RV32IDecoderTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class RV32IBitPatternMatcherTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};
