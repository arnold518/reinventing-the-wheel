#pragma once
#include "tests/ComponentTestModel.hpp"

#define SRAM_SCENARIO_TEST(Name) class Name : public circuit::test::ComponentScenarioTest { public: Name(); };
SRAM_SCENARIO_TEST(SRAMCellTest)
SRAM_SCENARIO_TEST(SRAMRowTest)
SRAM_SCENARIO_TEST(SRAMBitlinePairTest)
SRAM_SCENARIO_TEST(SRAMRowDecoderTest)
SRAM_SCENARIO_TEST(SRAMPrechargeTest)
SRAM_SCENARIO_TEST(SRAMWriteDriverTest)
SRAM_SCENARIO_TEST(SRAMSenseAmplifierTest)
SRAM_SCENARIO_TEST(SRAMColumnSelectorTest)
SRAM_SCENARIO_TEST(SRAMAccessControlTest)
SRAM_SCENARIO_TEST(SRAMOutputBufferTest)
SRAM_SCENARIO_TEST(SRAMStorageBankTest)
SRAM_SCENARIO_TEST(SRAMTest)
#undef SRAM_SCENARIO_TEST
class SRAMContractTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "SRAMContractTest"; }
protected: void verifyResults() override;
};
class SRAMTimingTest : public StandaloneVerificationTest {
public: std::string getTestName() const override { return "SRAMTimingTest"; }
protected: void verifyResults() override;
};
circuit::test::ComponentTestSpec sramDeviceSpec(size_t width, size_t depth, size_t words_per_row);
