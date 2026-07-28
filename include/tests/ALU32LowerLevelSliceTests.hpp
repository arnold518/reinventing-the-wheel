#pragma once

#include "modules/composite/AddSub32.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/Comparator32.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "tests/TruthTableComponentTest.hpp"
#include "tests/ComponentTestModel.hpp"

class Adder32Test : public TruthTableComponentTest<Adder32> {
public:
    Adder32Test();
};

class AddSub32Test : public TruthTableComponentTest<AddSub32> {
public:
    AddSub32Test();
};

class Logic32Test : public TruthTableComponentTest<Logic32> {
public:
    Logic32Test();
};

class ZeroDetect32Test : public TruthTableComponentTest<ZeroDetect32> {
public:
    ZeroDetect32Test();
};

class Comparator32Test : public TruthTableComponentTest<Comparator32> {
public:
    Comparator32Test();
};

class Shifter32Test : public TruthTableComponentTest<Shifter32> {
public:
    Shifter32Test();
};

class ALU32Test : public circuit::test::ComponentScenarioTest {
public:
    ALU32Test();
};

class ALU32RepresentativeSliceTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};
