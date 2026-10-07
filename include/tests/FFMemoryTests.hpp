#pragma once
#include "tests/ComponentTestModel.hpp"
#include "tests/TestHelpers.hpp"
class FFMemoryTest : public circuit::test::ComponentScenarioTest {
public: FFMemoryTest();
};
class FFWordTest : public circuit::test::ComponentScenarioTest {
public: FFWordTest();
};
class FFStorageBankTest : public circuit::test::ComponentScenarioTest {
public: FFStorageBankTest();
};
class FFStorageBankContractTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "FFStorageBankContractTest"; }
protected: void verifyResults() override;
};
class FFWordContractTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "FFWordContractTest"; }
protected: void verifyResults() override;
};
class FFMemoryFidelityTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "FFMemoryFidelityTest"; }
protected: void verifyResults() override;
};
// Shared deterministic workload for tests and the implementation benchmark.
circuit::test::ComponentTestSpec ffMemorySpec(size_t width, size_t depth);
circuit::BuildProfile ffMemoryProfile(const std::string& mode, size_t width,
                                     const std::string& root);
class FFMemoryContractTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "FFMemoryContractTest"; }
protected: void verifyResults() override;
};
class MemoryStorageTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "MemoryStorageTest"; }
protected: void verifyResults() override;
};
