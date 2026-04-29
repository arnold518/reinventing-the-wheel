#pragma once

#include "modules/composite/ALU8.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "tests/ComponentTruthTableTest.hpp"
#include "tests/TestHelpers.hpp"
#include <string>
#include <vector>

class HalfAdderTest : public ComponentTruthTableTest<HalfAdder> {
public:
    HalfAdderTest();

protected:
    std::vector<TruthRow> getTruthTable() const override;
};

class FullAdderTest : public ComponentTruthTableTest<FullAdder> {
public:
    FullAdderTest();

protected:
    std::vector<TruthRow> getTruthTable() const override;
};

class Logic8Test : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class MuxTest : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class Arithmetic8Test : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class Comparator8Test : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class Shifter8Test : public StandaloneVerificationTest {
public:
    std::string getTestName() const override;

protected:
    void verifyResults() override;
};

class Adder8Test : public ComponentTruthTableTest<Adder8> {
public:
    Adder8Test();

protected:
    std::vector<TruthRow> getTruthTable() const override;
};

class ZeroDetect8Test : public ComponentTruthTableTest<ZeroDetect8> {
public:
    ZeroDetect8Test();

protected:
    std::vector<TruthRow> getTruthTable() const override;
};

class ALU8Test : public ComponentTruthTableTest<ALU8> {
public:
    ALU8Test();

protected:
    std::vector<TruthRow> getTruthTable() const override;
};
