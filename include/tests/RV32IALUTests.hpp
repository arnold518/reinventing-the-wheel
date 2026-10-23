#pragma once
#include "modules/composite/WordComponent.hpp"
#include "modules/rv32i/RV32IALU.hpp"
#include "tests/TruthTableComponentTest.hpp"
#include "tests/ComponentTestModel.hpp"
// 32-bit Chapter 2 references used by the RV32I ALU.
class ParallelPrefixAdderWidth32Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::PrefixAdder,32>> {
public:
    ParallelPrefixAdderWidth32Test();
};
class ZeroDetectWidth32Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Zero,32>> {
public:
    ZeroDetectWidth32Test();
};
// RV32IALU and its parts.
class RV32IALUAddSubTest : public TruthTableComponentTest<RV32IALUAddSub> { public: RV32IALUAddSubTest(); };
class RV32IALULogicTest : public TruthTableComponentTest<RV32IALULogic> { public: RV32IALULogicTest(); };
class RV32IALUShifterTest : public TruthTableComponentTest<RV32IALUShifter> { public: RV32IALUShifterTest(); };
class RV32IALUControlTest : public TruthTableComponentTest<RV32IALUControl> { public: RV32IALUControlTest(); };
class RV32IALUCompareTest : public TruthTableComponentTest<RV32IALUCompare> { public: RV32IALUCompareTest(); };
class RV32IALUResultSelectTest : public TruthTableComponentTest<RV32IALUResultSelect> { public: RV32IALUResultSelectTest(); };
class RV32IALUTest : public circuit::test::ComponentScenarioTest {
public:
    RV32IALUTest();
};
class RV32IALUSliceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;
protected:
    void verifyResults() override;
};
