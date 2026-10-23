#pragma once
#include "tests/TruthTableComponentTest.hpp"
#include "modules/composite/WordComponent.hpp"
class ALUControlDecoderTest : public TruthTableComponentTest<ALUControlTestTarget> { public: ALUControlDecoderTest(); };
class ALUResultSelectorWidth8Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Result,8>> { public: ALUResultSelectorWidth8Test(); };
class ALUHierarchyTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override { return "ALUHierarchyTest"; }
protected:
    void verifyResults() override;
};
