#pragma once

#include "modules/composite/AddSub32.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/Comparator32.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "tests/ComponentFamilyRowsTest.hpp"
#include "tests/ComponentRowsTest.hpp"

class Adder32Test : public ComponentRowsTest<Adder32> {
public:
    Adder32Test();
};

class AddSub32Test : public ComponentRowsTest<AddSub32> {
public:
    AddSub32Test();
};

class Logic32Test : public ComponentRowsTest<Logic32> {
public:
    Logic32Test();
};

class ZeroDetect32Test : public ComponentRowsTest<ZeroDetect32> {
public:
    ZeroDetect32Test();
};

class Comparator32Test : public ComponentRowsTest<Comparator32> {
public:
    Comparator32Test();
};

class Shifter32Test : public ComponentRowsTest<Shifter32> {
public:
    Shifter32Test();
};

class ALU32StructuralContractTest : public ComponentFamilyRowsTest {
public:
    ALU32StructuralContractTest();
};

class ALU32BehavioralContractTest : public ComponentFamilyRowsTest {
public:
    ALU32BehavioralContractTest();
};

class ALU32LowerLevelSliceTest : public ComponentRowsTest<ALU32> {
public:
    ALU32LowerLevelSliceTest();

protected:
    void verifyResults() override;
};
