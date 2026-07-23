#pragma once

#include "tests/TestHelpers.hpp"

class RV32IControlFlowUnitEquivalenceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class RV32IDecodeControlUnitEquivalenceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class RV32IRegisterFileEquivalenceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class ALU32EquivalenceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class RV32IExecutionControlStatusUnitEquivalenceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};
