#pragma once

#include "tests/TestHelpers.hpp"

class BuildProfileTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class ComponentCatalogSelectionTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class BuiltinComponentCatalogInventoryTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class RecursiveBuildProfileTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class MemoryBitEquivalenceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class Register32EquivalenceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};
