#pragma once
#include "tests/TruthTableComponentTest.hpp"
#include "modules/composite/ALU8ControlDecoder.hpp"
#include "modules/composite/ALU8ResultSelector.hpp"
#include "modules/composite/ALU32ControlDecoder.hpp"
#include "modules/composite/ALU32CompareUnit.hpp"
#include "modules/composite/ALU32ResultSelector.hpp"
#include "modules/composite/ALU32FlagUnit.hpp"

class ALU8ControlDecoderTest : public TruthTableComponentTest<ALU8ControlDecoder> { public: ALU8ControlDecoderTest(); };
class ALU8ResultSelectorTest : public TruthTableComponentTest<ALU8ResultSelector> { public: ALU8ResultSelectorTest(); };
class ALU32ControlDecoderTest : public TruthTableComponentTest<ALU32ControlDecoder> { public: ALU32ControlDecoderTest(); };
class ALU32CompareUnitTest : public TruthTableComponentTest<ALU32CompareUnit> { public: ALU32CompareUnitTest(); };
class ALU32ResultSelectorTest : public TruthTableComponentTest<ALU32ResultSelector> { public: ALU32ResultSelectorTest(); };
class ALU32FlagUnitTest : public TruthTableComponentTest<ALU32FlagUnit> { public: ALU32FlagUnitTest(); };
class ALUHierarchyTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "ALUHierarchyTest"; }
protected:
    void verifyResults() override;
};
