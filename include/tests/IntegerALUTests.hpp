#pragma once

#include "modules/composite/WordDatapath.hpp"
#include "tests/TruthTableComponentTest.hpp"
#include "tests/ComponentTestModel.hpp"

class AdderPrefixWidth32Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Adder,32,circuit::word::Operations::Extended,circuit::word::AdderTopology::ParallelPrefix>> {
public:
    AdderPrefixWidth32Test();
};

class AddSubIntegerWidth32Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::AddSub,32,circuit::word::Operations::IntegerDatapath,circuit::word::AdderTopology::ParallelPrefix>> {
public:
    AddSubIntegerWidth32Test();
};

class LogicIntegerWidth32Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Logic,32,circuit::word::Operations::IntegerDatapath,circuit::word::AdderTopology::Ripple>> {
public:
    LogicIntegerWidth32Test();
};

class ZeroDetectWidth32Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Zero,32,circuit::word::Operations::Extended,circuit::word::AdderTopology::Ripple>> {
public:
    ZeroDetectWidth32Test();
};

class ShiftIntegerWidth32Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Shift,32,circuit::word::Operations::IntegerDatapath,circuit::word::AdderTopology::Ripple>> {
public:
    ShiftIntegerWidth32Test();
};

class ALUIntegerWidth32Test : public circuit::test::ComponentScenarioTest {
public:
    ALUIntegerWidth32Test();
};

class ALUIntegerCoreWidth32Test : public circuit::test::ComponentScenarioTest {
public:
    ALUIntegerCoreWidth32Test();
};

class IntegerALUSliceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};
