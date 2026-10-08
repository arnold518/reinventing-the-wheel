#pragma once

#include "modules/basic/Decoder.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/ALU8.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/composite/HalfAdder.hpp"
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

class Mux32to1Test : public TruthTableComponentTest<Mux32to1> {
public:
    Mux32to1Test();
};

class Mux2to1_4bitTest : public TruthTableComponentTest<Mux2to1_4bit> {
public:
    Mux2to1_4bitTest();
};

class Mux2to1_32bitTest : public TruthTableComponentTest<Mux2to1_32bit> {
public:
    Mux2to1_32bitTest();
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

class Mux32to1_32bitTest : public TruthTableComponentTest<Mux32to1_32bit> {
public:
    Mux32to1_32bitTest();
};

class Decoder5to32Test : public TruthTableComponentTest<Decoder5to32> {
public:
    Decoder5to32Test();
};

class Adder8Test : public TruthTableComponentTest<Adder8> {
public:
    Adder8Test();
};

class ZeroDetect8Test : public TruthTableComponentTest<ZeroDetect8> {
public:
    ZeroDetect8Test();
};

class ALU8Test : public TruthTableComponentTest<ALU8> {
public:
    ALU8Test();
};
