#pragma once

#include "modules/basic/Decoder.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/WordComponent.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/composite/HalfAdder.hpp"
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

class Mux2Width1Test : public TruthTableComponentTest<Mux<2,1>> {
public:
    Mux2Width1Test();
};

class Mux4Width1Test : public TruthTableComponentTest<Mux<4,1>> {
public:
    Mux4Width1Test();
};

class Mux8Width1Test : public TruthTableComponentTest<Mux<8,1>> {
public:
    Mux8Width1Test();
};

class Mux32Width1Test : public TruthTableComponentTest<Mux<32,1>> {
public:
    Mux32Width1Test();
};

class Mux2Width4Test : public TruthTableComponentTest<Mux<2,4>> {
public:
    Mux2Width4Test();
};

class Mux2Width32Test : public TruthTableComponentTest<Mux<2,32>> {
public:
    Mux2Width32Test();
};

class Mux4Width32Test : public TruthTableComponentTest<Mux<4,32>> {
public:
    Mux4Width32Test();
};

class Mux8Width8Test : public TruthTableComponentTest<Mux<8,8>> {
public:
    Mux8Width8Test();
};

class Mux8Width32Test : public TruthTableComponentTest<Mux<8,32>> {
public:
    Mux8Width32Test();
};

class Mux32Width32Test : public TruthTableComponentTest<Mux<32,32>> {
public:
    Mux32Width32Test();
};

class DecoderWidth5Test : public TruthTableComponentTest<Decoder<5>> {
public:
    DecoderWidth5Test();
};

class DecoderWidth5ZeroDisabledTest : public TruthTableComponentTest<Decoder<5>> {
public:
    DecoderWidth5ZeroDisabledTest();
};

class RippleCarryAdderWidth8Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::RippleAdder,8>> {
public:
    RippleCarryAdderWidth8Test();
};

class ZeroDetectWidth8Test : public TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Zero,8>> {
public:
    ZeroDetectWidth8Test();
};
