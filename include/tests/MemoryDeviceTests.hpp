#pragma once
#include "tests/ComponentTestModel.hpp"
#define MEMORY_DEVICE_TEST(Name)                                                                             \
    class Name : public circuit::test::ComponentScenarioTest {                                               \
      public:                                                                                                \
        Name();                                                                                              \
    };
MEMORY_DEVICE_TEST(MatrixRowDecoderTest)
MEMORY_DEVICE_TEST(MatrixColumnMuxTest)
MEMORY_DEVICE_TEST(MatrixWriteDistributorTest)
MEMORY_DEVICE_TEST(StaticReadColumnTest)
MEMORY_DEVICE_TEST(MemoryOutputBufferTest)
MEMORY_DEVICE_TEST(ROMCellTest)
MEMORY_DEVICE_TEST(ROMRowTest)
MEMORY_DEVICE_TEST(ROMAddressControlTest)
MEMORY_DEVICE_TEST(ROMStorageBankTest)
MEMORY_DEVICE_TEST(ROMTest)
MEMORY_DEVICE_TEST(DRAMCellTest)
MEMORY_DEVICE_TEST(DRAMRowTest)
MEMORY_DEVICE_TEST(DRAMSenseColumnTest)
MEMORY_DEVICE_TEST(DRAMStorageBankTest)
MEMORY_DEVICE_TEST(DRAMCommandControlTest)
MEMORY_DEVICE_TEST(DRAMTest)
MEMORY_DEVICE_TEST(NORFlashCellTest)
MEMORY_DEVICE_TEST(NORFlashRowTest)
MEMORY_DEVICE_TEST(NORFlashStorageBankTest)
MEMORY_DEVICE_TEST(NORFlashCommandControlTest)
MEMORY_DEVICE_TEST(NORFlashTest)
#undef MEMORY_DEVICE_TEST
class MemoryHelperContractTest : public StandaloneVerificationTest {
  public:
    std::string getTestName() const override { return "MemoryHelperContractTest"; }
  protected:
    void verifyResults() override;
};
class MemoryDeviceContractTest : public StandaloneVerificationTest {
  public:
    std::string getTestName() const override { return "MemoryDeviceContractTest"; }

  protected:
    void verifyResults() override;
};
class MemoryDeviceTimingTest : public StandaloneVerificationTest {
  public:
    std::string getTestName() const override { return "MemoryDeviceTimingTest"; }

  protected:
    void verifyResults() override;
};
