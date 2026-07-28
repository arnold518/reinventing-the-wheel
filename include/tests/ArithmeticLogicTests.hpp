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
#include "tests/TruthTableComponentTest.hpp"
#include <string>
#include <vector>

class HalfAdderTest : public TruthTableComponentTest<HalfAdder> {
public:
    HalfAdderTest();
};

class FullAdderTest : public TruthTableComponentTest<FullAdder> {
public:
    FullAdderTest();
};

class AND8Test : public TruthTableComponentTest<AND8> {
public:
    AND8Test();
};

class OR8Test : public TruthTableComponentTest<OR8> {
public:
    OR8Test();
};

class XOR8Test : public TruthTableComponentTest<XOR8> {
public:
    XOR8Test();
};

class NOT8Test : public TruthTableComponentTest<NOT8> {
public:
    NOT8Test();
};

class NAND8Test : public TruthTableComponentTest<NAND8> {
public:
    NAND8Test();
};

class NOR8Test : public TruthTableComponentTest<NOR8> {
public:
    NOR8Test();
};

class Mux2to1Test : public TruthTableComponentTest<Mux2to1> {
public:
    Mux2to1Test();
};

class Mux4to1Test : public TruthTableComponentTest<Mux4to1> {
public:
    Mux4to1Test();
};

class Mux8to1Test : public TruthTableComponentTest<Mux8to1> {
public:
    Mux8to1Test();
};

class Mux16to1Test : public TruthTableComponentTest<Mux16to1> {
public:
    Mux16to1Test();
};

class Mux32to1Test : public TruthTableComponentTest<Mux32to1> {
public:
    Mux32to1Test();
};

class Mux2to1_8bitTest : public TruthTableComponentTest<Mux2to1_8bit> {
public:
    Mux2to1_8bitTest();
};

class Mux2to1_4bitTest : public TruthTableComponentTest<Mux2to1_4bit> {
public:
    Mux2to1_4bitTest();
};

class Mux2to1_32bitTest : public TruthTableComponentTest<Mux2to1_32bit> {
public:
    Mux2to1_32bitTest();
};

class Mux4to1_8bitTest : public TruthTableComponentTest<Mux4to1_8bit> {
public:
    Mux4to1_8bitTest();
};

class Mux4to1_32bitTest : public TruthTableComponentTest<Mux4to1_32bit> {
public:
    Mux4to1_32bitTest();
};

class Mux8to1_8bitTest : public TruthTableComponentTest<Mux8to1_8bit> {
public:
    Mux8to1_8bitTest();
};

class Mux8to1_32bitTest : public TruthTableComponentTest<Mux8to1_32bit> {
public:
    Mux8to1_32bitTest();
};

class Mux16to1_8bitTest : public TruthTableComponentTest<Mux16to1_8bit> {
public:
    Mux16to1_8bitTest();
};

class Mux32to1_32bitTest : public TruthTableComponentTest<Mux32to1_32bit> {
public:
    Mux32to1_32bitTest();
};

class Decoder2to4Test : public TruthTableComponentTest<Decoder2to4> {
public:
    Decoder2to4Test();
};

class Decoder5to32Test : public TruthTableComponentTest<Decoder5to32> {
public:
    Decoder5to32Test();
};

class Adder8Test : public TruthTableComponentTest<Adder8> {
public:
    Adder8Test();
};

class TwosComplement8Test : public TruthTableComponentTest<TwosComplement8> {
public:
    TwosComplement8Test();
};

class Subtractor8Test : public TruthTableComponentTest<Subtractor8> {
public:
    Subtractor8Test();
};

class SubtractorWithBorrow8Test : public TruthTableComponentTest<SubtractorWithBorrow8> {
public:
    SubtractorWithBorrow8Test();
};

class Incrementer8Test : public TruthTableComponentTest<Incrementer8> {
public:
    Incrementer8Test();
};

class Decrementer8Test : public TruthTableComponentTest<Decrementer8> {
public:
    Decrementer8Test();
};

class EqualityChecker8Test : public TruthTableComponentTest<EqualityChecker8> {
public:
    EqualityChecker8Test();
};

class Comparator8Test : public TruthTableComponentTest<Comparator8> {
public:
    Comparator8Test();
};

class SignedComparator8Test : public TruthTableComponentTest<SignedComparator8> {
public:
    SignedComparator8Test();
};

class ShiftLeftLogical8Test : public TruthTableComponentTest<ShiftLeftLogical8> {
public:
    ShiftLeftLogical8Test();
};

class ShiftRightLogical8Test : public TruthTableComponentTest<ShiftRightLogical8> {
public:
    ShiftRightLogical8Test();
};

class ShiftRightArithmetic8Test : public TruthTableComponentTest<ShiftRightArithmetic8> {
public:
    ShiftRightArithmetic8Test();
};

class ZeroDetect8Test : public TruthTableComponentTest<ZeroDetect8> {
public:
    ZeroDetect8Test();
};

class ALU8Test : public TruthTableComponentTest<ALU8> {
public:
    ALU8Test();
};
