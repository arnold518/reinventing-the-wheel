#pragma once
#include "tests/ComponentTestModel.hpp"

#define DECLARE_STAGE_TEST(Name) \
class RV32ISingleCycle##Name##StageTest : public circuit::test::ComponentScenarioTest { \
public: RV32ISingleCycle##Name##StageTest(); };
DECLARE_STAGE_TEST(Fetch)
DECLARE_STAGE_TEST(Decode)
DECLARE_STAGE_TEST(Execute)
DECLARE_STAGE_TEST(Memory)
DECLARE_STAGE_TEST(Writeback)
DECLARE_STAGE_TEST(NextPC)
#undef DECLARE_STAGE_TEST

class RV32ISingleCycleStageIntegrationTest : public StandaloneVerificationTest {
protected:
    std::string getTestName() const override { return "RV32ISingleCycleStageIntegrationTest"; }
    void verifyResults() override;
};
