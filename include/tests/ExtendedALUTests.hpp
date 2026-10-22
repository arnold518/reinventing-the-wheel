#pragma once
#include "modules/composite/WordDatapath.hpp"
#include "tests/TruthTableComponentTest.hpp"

class AddSubExtendedWidth8Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::AddSub,8,circuit::word::Operations::Extended,circuit::word::AdderTopology::Ripple>> { public: AddSubExtendedWidth8Test(); };
class ArithmeticExtendedWidth8Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Arithmetic,8,circuit::word::Operations::Extended,circuit::word::AdderTopology::Ripple>> { public: ArithmeticExtendedWidth8Test(); };
class LogicExtendedWidth8Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Logic,8,circuit::word::Operations::Extended,circuit::word::AdderTopology::Ripple>> { public: LogicExtendedWidth8Test(); };
class ShiftExtendedWidth8Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Shift,8,circuit::word::Operations::Extended,circuit::word::AdderTopology::Ripple>> { public: ShiftExtendedWidth8Test(); };
class CompareExtendedWidth8Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Compare,8,circuit::word::Operations::Extended,circuit::word::AdderTopology::Ripple>> { public: CompareExtendedWidth8Test(); };
class FlagsExtendedWidth8Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Flags,8,circuit::word::Operations::Extended,circuit::word::AdderTopology::Ripple>> { public: FlagsExtendedWidth8Test(); };
class ExtendedALUContractTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "ExtendedALUContractTest"; }
protected:
    void verifyResults() override;
};
class ExtendedALUFidelityContractTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "ExtendedALUFidelityContractTest"; }
protected:
    void verifyResults() override;
};
