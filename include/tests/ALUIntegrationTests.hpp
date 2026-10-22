#pragma once
#include "tests/TruthTableComponentTest.hpp"
#include "modules/composite/WordDatapath.hpp"

class ALUControlExtendedTest : public TruthTableComponentTest<ControlTestTarget<circuit::word::Operations::Extended>> { public: ALUControlExtendedTest(); };
class ResultSelectorExtendedWidth8Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Result,8,circuit::word::Operations::Extended,circuit::word::AdderTopology::Ripple>> { public: ResultSelectorExtendedWidth8Test(); };
class ALUControlIntegerTest : public TruthTableComponentTest<ControlTestTarget<circuit::word::Operations::IntegerDatapath>> { public: ALUControlIntegerTest(); };
class CompareIntegerWidth32Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Compare,32,circuit::word::Operations::IntegerDatapath,circuit::word::AdderTopology::Ripple>> { public: CompareIntegerWidth32Test(); };
class ResultSelectorIntegerWidth32Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Result,32,circuit::word::Operations::IntegerDatapath,circuit::word::AdderTopology::Ripple>> { public: ResultSelectorIntegerWidth32Test(); };
class FlagsIntegerWidth32Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Flags,32,circuit::word::Operations::IntegerDatapath,circuit::word::AdderTopology::Ripple>> { public: FlagsIntegerWidth32Test(); };
class ALUHierarchyTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "ALUHierarchyTest"; }
protected:
    void verifyResults() override;
};
