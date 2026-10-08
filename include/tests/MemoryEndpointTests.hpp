#pragma once
#include "tests/ComponentTestModel.hpp"
#define ENDPOINT_TEST(Name) class Name : public circuit::test::ComponentScenarioTest { public: Name(); };
ENDPOINT_TEST(MemoryTransactionPortTest)
ENDPOINT_TEST(MemoryResponseBufferTest)
ENDPOINT_TEST(MemoryControllerTest)
ENDPOINT_TEST(FFAlwaysReadyControllerTest)
ENDPOINT_TEST(MemoryEndpointFFTest)
ENDPOINT_TEST(MemoryEndpointSRAMTest)
ENDPOINT_TEST(MemoryEndpointDRAMTest)
ENDPOINT_TEST(MemoryEndpointROMTest)
ENDPOINT_TEST(MemoryEndpointFlashTest)
ENDPOINT_TEST(ScalableMemoryEndpointTest)
ENDPOINT_TEST(NativeMemoryContainerTest)
#undef ENDPOINT_TEST
class RegisterWidthsTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "RegisterWidthsTest"; }
protected: void verifyResults() override;
};
class ClockedStateBitTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "ClockedStateBitTest"; }
protected: void verifyResults() override;
};
class NativeMemoryDeviceContractTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "NativeMemoryDeviceContractTest"; }
protected: void verifyResults() override;
};
class MemoryEndpointContractTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "MemoryEndpointContractTest"; }
protected: void verifyResults() override;
};
class MemoryEndpointTimingTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "MemoryEndpointTimingTest"; }
protected: void verifyResults() override;
};
