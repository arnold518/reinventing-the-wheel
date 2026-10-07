#pragma once
#include "tests/ComponentTestModel.hpp"
#define BASELINE_TEST(Name) class RV32IBaseline##Name##Test : public circuit::test::ComponentScenarioTest {public: RV32IBaseline##Name##Test();};
BASELINE_TEST(FetchStage)
BASELINE_TEST(DecodeStage)
BASELINE_TEST(ExecuteStage)
BASELINE_TEST(MemoryStage)
BASELINE_TEST(DependencyUnit)
BASELINE_TEST(Control)
#undef BASELINE_TEST
class RV32IBaselinePipelineCoreTest : public circuit::test::ComponentScenarioTest {
public:
    RV32IBaselinePipelineCoreTest();
    bool run() override;
};
