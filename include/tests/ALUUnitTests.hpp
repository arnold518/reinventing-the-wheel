#pragma once
#include "modules/composite/WordComponent.hpp"
#include "tests/TruthTableComponentTest.hpp"
using circuit::word::Unit;
class AddSubWidth8Test : public TruthTableComponentTest<WordTestTarget<Unit::AddSub,8>> { public: AddSubWidth8Test(); };
class ArithmeticUnitWidth8Test : public TruthTableComponentTest<WordTestTarget<Unit::Arithmetic,8>> { public: ArithmeticUnitWidth8Test(); };
class LogicUnitWidth8Test : public TruthTableComponentTest<WordTestTarget<Unit::Logic,8>> { public: LogicUnitWidth8Test(); };
class BarrelShifterWidth8Test : public TruthTableComponentTest<WordTestTarget<Unit::Shift,8>> { public: BarrelShifterWidth8Test(); };
class CompareSelectWidth8Test : public TruthTableComponentTest<WordTestTarget<Unit::Compare,8>> { public: CompareSelectWidth8Test(); };
class ALUFlagUnitWidth8Test : public TruthTableComponentTest<WordTestTarget<Unit::Flags,8>> { public: ALUFlagUnitWidth8Test(); };
class ALUContractTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "ALUContractTest"; }
protected:
    void verifyResults() override;
};
class ALUFidelityContractTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "ALUFidelityContractTest"; }
protected:
    void verifyResults() override;
};
