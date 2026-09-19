#pragma once
#include "tests/ComponentTestModel.hpp"
#define ENDPOINT_TEST(Name) class Name : public circuit::test::ComponentScenarioTest { public: Name(); };
ENDPOINT_TEST(MemoryTransactionPortTest)
ENDPOINT_TEST(MemoryControllerTest)
ENDPOINT_TEST(MemoryEndpointFFTest)
ENDPOINT_TEST(MemoryEndpointSRAMTest)
ENDPOINT_TEST(MemoryEndpointDRAMTest)
ENDPOINT_TEST(MemoryEndpointROMTest)
ENDPOINT_TEST(MemoryEndpointFlashTest)
ENDPOINT_TEST(MemoryEndpointRelayTest)
ENDPOINT_TEST(ScalableMemoryContainerTest)
#undef ENDPOINT_TEST
class MemoryEndpointContractTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "MemoryEndpointContractTest"; }
protected: void verifyResults() override;
};
class MemoryEndpointTimingTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "MemoryEndpointTimingTest"; }
protected: void verifyResults() override;
};
