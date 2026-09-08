#pragma once
#include "modules/composite/AddSub8.hpp"
#include "modules/composite/ArithmeticUnit8.hpp"
#include "modules/composite/LogicUnit8.hpp"
#include "modules/composite/BarrelShifter8.hpp"
#include "modules/composite/CompareSelect8.hpp"
#include "modules/composite/ALU8FlagUnit.hpp"
#include "tests/TruthTableComponentTest.hpp"

class AddSub8Test : public TruthTableComponentTest<AddSub8> { public: AddSub8Test(); };
class ArithmeticUnit8Test : public TruthTableComponentTest<ArithmeticUnit8> { public: ArithmeticUnit8Test(); };
class LogicUnit8Test : public TruthTableComponentTest<LogicUnit8> { public: LogicUnit8Test(); };
class BarrelShifter8Test : public TruthTableComponentTest<BarrelShifter8> { public: BarrelShifter8Test(); };
class CompareSelect8Test : public TruthTableComponentTest<CompareSelect8> { public: CompareSelect8Test(); };
class ALU8FlagUnitTest : public TruthTableComponentTest<ALU8FlagUnit> { public: ALU8FlagUnitTest(); };
class ALU8ContractTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "ALU8ContractTest"; }
protected:
    void verifyResults() override;
};
class ALU8FidelityContractTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "ALU8FidelityContractTest"; }
protected:
    void verifyResults() override;
};
