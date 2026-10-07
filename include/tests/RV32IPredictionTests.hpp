#pragma once
#include "tests/ComponentTestModel.hpp"
#define PREDICTION_TEST(Name) \
class Name : public circuit::test::ComponentScenarioTest { public: Name(); };
PREDICTION_TEST(RV32IPredictionCounterTest)
PREDICTION_TEST(RV32IBranchPredecodeTest)
PREDICTION_TEST(RV32IBranchPredictorTest)
PREDICTION_TEST(RV32IBranchRecoveryTest)
PREDICTION_TEST(RV32IPredictionPathTest)
#undef PREDICTION_TEST
