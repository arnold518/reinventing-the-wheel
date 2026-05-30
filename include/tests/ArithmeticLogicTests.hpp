#pragma once

#include "modules/basic/Decoder.hpp"
#include "modules/basic/Logic8.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/ALU8.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/Arithmetic8.hpp"
#include "modules/composite/Comparator8.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/composite/Shifter8.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "tests/ComponentRowsTest.hpp"
#include "tests/ComponentTruthTableTest.hpp"
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

class AND8Test : public ComponentRowsTest<AND8> {
public:
    AND8Test();
};

class OR8Test : public ComponentRowsTest<OR8> {
public:
    OR8Test();
};

class XOR8Test : public ComponentRowsTest<XOR8> {
public:
    XOR8Test();
};

class NOT8Test : public ComponentRowsTest<NOT8> {
public:
    NOT8Test();
};

class NAND8Test : public ComponentRowsTest<NAND8> {
public:
    NAND8Test();
};

class NOR8Test : public ComponentRowsTest<NOR8> {
public:
    NOR8Test();
};

class Mux2to1Test : public ComponentRowsTest<Mux2to1> {
public:
    Mux2to1Test();
};

class Mux4to1Test : public ComponentRowsTest<Mux4to1> {
public:
    Mux4to1Test();
};

class Mux8to1Test : public ComponentRowsTest<Mux8to1> {
public:
    Mux8to1Test();
};

class Mux16to1Test : public ComponentRowsTest<Mux16to1> {
public:
    Mux16to1Test();
};

class Mux32to1Test : public ComponentRowsTest<Mux32to1> {
public:
    Mux32to1Test();
};

class Mux2to1_8bitTest : public ComponentRowsTest<Mux2to1_8bit> {
public:
    Mux2to1_8bitTest();
};

class Mux4to1_8bitTest : public ComponentRowsTest<Mux4to1_8bit> {
public:
    Mux4to1_8bitTest();
};

class Mux4to1_32bitTest : public ComponentRowsTest<Mux4to1_32bit> {
public:
    Mux4to1_32bitTest();
};

class Mux8to1_8bitTest : public ComponentRowsTest<Mux8to1_8bit> {
public:
    Mux8to1_8bitTest();
};

class Mux16to1_8bitTest : public ComponentRowsTest<Mux16to1_8bit> {
public:
    Mux16to1_8bitTest();
};

class Mux32to1_32bitTest : public ComponentRowsTest<Mux32to1_32bit> {
public:
    Mux32to1_32bitTest();
};

class Decoder2to4Test : public ComponentRowsTest<Decoder2to4> {
public:
    Decoder2to4Test();
};

class Decoder5to32Test : public ComponentRowsTest<Decoder5to32> {
public:
    Decoder5to32Test();
};

class Adder8Test : public ComponentTruthTableTest<Adder8> {
public:
    Adder8Test();

protected:
    std::vector<TruthRow> getTruthTable() const override;
};

class TwosComplement8Test : public ComponentRowsTest<TwosComplement8> {
public:
    TwosComplement8Test();
};

class Subtractor8Test : public ComponentRowsTest<Subtractor8> {
public:
    Subtractor8Test();
};

class SubtractorWithBorrow8Test : public ComponentRowsTest<SubtractorWithBorrow8> {
public:
    SubtractorWithBorrow8Test();
};

class Incrementer8Test : public ComponentRowsTest<Incrementer8> {
public:
    Incrementer8Test();
};

class Decrementer8Test : public ComponentRowsTest<Decrementer8> {
public:
    Decrementer8Test();
};

class EqualityChecker8Test : public ComponentRowsTest<EqualityChecker8> {
public:
    EqualityChecker8Test();
};

class Comparator8Test : public ComponentRowsTest<Comparator8> {
public:
    Comparator8Test();
};

class SignedComparator8Test : public ComponentRowsTest<SignedComparator8> {
public:
    SignedComparator8Test();
};

class ShiftLeftLogical8Test : public ComponentRowsTest<ShiftLeftLogical8> {
public:
    ShiftLeftLogical8Test();
};

class ShiftRightLogical8Test : public ComponentRowsTest<ShiftRightLogical8> {
public:
    ShiftRightLogical8Test();
};

class ShiftRightArithmetic8Test : public ComponentRowsTest<ShiftRightArithmetic8> {
public:
    ShiftRightArithmetic8Test();
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
