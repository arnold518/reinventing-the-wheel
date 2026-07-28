#pragma once

#include "tests/ComponentTestModel.hpp"
#include "tests/TestHelpers.hpp"
#include <string>

class RV32IDecoderTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class RV32IBitPatternMatcherTest
    : public circuit::test::ComponentScenarioTest {
public:
    RV32IBitPatternMatcherTest();
    bool run() override;
};
