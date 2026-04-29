#include "tests/ArithmeticLogicTests.hpp"

#include "modules/basic/Logic8.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Arithmetic8.hpp"
#include "modules/composite/Comparator8.hpp"
#include "modules/composite/Shifter8.hpp"
#include "tests/TestHelpers.hpp"
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {
void expect(bool condition, const char* test_name) {
    if (!condition) {
        std::cerr << test_name << " failed" << std::endl;
        assert(false && "Arithmetic/logic test failed");
    }
}

PinValue pinBits(uint64_t value) {
    return PinValue(value);
}

PinValue pinBit(bool value) {
    return PinValue(value ? LogicValue::HIGH : LogicValue::LOW);
}

PinValue pinLogic(LogicValue value) {
    return PinValue(value);
}

TruthRow adderRow(uint64_t a, uint64_t b, bool cin, uint64_t sum, bool cout) {
    return {{{"A", pinBits(a)}, {"B", pinBits(b)}, {"Cin", pinBit(cin)}},
            {{"Sum", pinBits(sum)}, {"Cout", pinBit(cout)}}};
}

TruthRow aluRow(uint64_t a, uint64_t b, uint64_t op, uint64_t out, bool zero, bool carry, bool overflow, bool negative) {
    return {{{"A", pinBits(a)}, {"B", pinBits(b)}, {"OP", pinBits(op)}},
            {{"OUT", pinBits(out)}, {"ZERO", pinBit(zero)}, {"CARRY", pinBit(carry)}, {"OVERFLOW", pinBit(overflow)}, {"NEGATIVE", pinBit(negative)}}};
}

std::string muxInputName(size_t input_count, size_t index) {
    if (input_count == 2) {
        return index == 0 ? "A" : "B";
    }
    return "IN" + std::to_string(index);
}

std::vector<TestRow> oneBitMuxRows(size_t input_count) {
    std::vector<TestRow> rows;
    for (size_t selected = 0; selected < input_count; ++selected) {
        std::map<std::string, TestValue> selected_high_inputs;
        for (size_t i = 0; i < input_count; ++i) {
            selected_high_inputs[muxInputName(input_count, i)] = bit(false);
        }
        selected_high_inputs[muxInputName(input_count, selected)] = bit(true);
        selected_high_inputs["SEL"] = bits(selected);
        rows.push_back({selected_high_inputs, {{"OUT", bit(true)}}});

        std::map<std::string, TestValue> unselected_high_inputs;
        for (size_t i = 0; i < input_count; ++i) {
            unselected_high_inputs[muxInputName(input_count, i)] = bit(false);
        }
        unselected_high_inputs[muxInputName(input_count, (selected + 1) % input_count)] = bit(true);
        unselected_high_inputs["SEL"] = bits(selected);
        rows.push_back({unselected_high_inputs, {{"OUT", bit(false)}}});
    }
    return rows;
}

uint64_t muxBusValue(size_t index) {
    return ((index * 0x11U) + 0x13U) & 0xFFU;
}

std::vector<TestRow> busMuxRows(size_t input_count) {
    std::vector<TestRow> rows;
    for (size_t selected = 0; selected < input_count; ++selected) {
        std::map<std::string, TestValue> inputs;
        for (size_t i = 0; i < input_count; ++i) {
            inputs[muxInputName(input_count, i)] = bits(muxBusValue(i));
        }
        inputs["SEL"] = bits(selected);
        rows.push_back({inputs, {{"OUT", bits(muxBusValue(selected))}}});
    }
    return rows;
}
}

HalfAdderTest::HalfAdderTest()
    : ComponentTruthTableTest<HalfAdder>("HalfAdderTest", "HA_ROOT") {}

std::vector<TruthRow> HalfAdderTest::getTruthTable() const {
    constexpr auto L = LogicValue::LOW;
    constexpr auto H = LogicValue::HIGH;

    return {
        TruthRow({{"A", pinLogic(L)}, {"B", pinLogic(L)}}, {{"Sum", pinLogic(L)}, {"Carry", pinLogic(L)}}),
        TruthRow({{"A", pinLogic(L)}, {"B", pinLogic(H)}}, {{"Sum", pinLogic(H)}, {"Carry", pinLogic(L)}}),
        TruthRow({{"A", pinLogic(H)}, {"B", pinLogic(L)}}, {{"Sum", pinLogic(H)}, {"Carry", pinLogic(L)}}),
        TruthRow({{"A", pinLogic(H)}, {"B", pinLogic(H)}}, {{"Sum", pinLogic(L)}, {"Carry", pinLogic(H)}}),
    };
}

FullAdderTest::FullAdderTest()
    : ComponentTruthTableTest<FullAdder>("FullAdderTest", "FA_ROOT") {}

std::vector<TruthRow> FullAdderTest::getTruthTable() const {
    constexpr auto L = LogicValue::LOW;
    constexpr auto H = LogicValue::HIGH;

    return {
        TruthRow({{"A", pinLogic(L)}, {"B", pinLogic(L)}, {"Carry_in", pinLogic(L)}}, {{"Sum", pinLogic(L)}, {"Carry_out", pinLogic(L)}}),
        TruthRow({{"A", pinLogic(L)}, {"B", pinLogic(L)}, {"Carry_in", pinLogic(H)}}, {{"Sum", pinLogic(H)}, {"Carry_out", pinLogic(L)}}),
        TruthRow({{"A", pinLogic(L)}, {"B", pinLogic(H)}, {"Carry_in", pinLogic(L)}}, {{"Sum", pinLogic(H)}, {"Carry_out", pinLogic(L)}}),
        TruthRow({{"A", pinLogic(L)}, {"B", pinLogic(H)}, {"Carry_in", pinLogic(H)}}, {{"Sum", pinLogic(L)}, {"Carry_out", pinLogic(H)}}),
        TruthRow({{"A", pinLogic(H)}, {"B", pinLogic(L)}, {"Carry_in", pinLogic(L)}}, {{"Sum", pinLogic(H)}, {"Carry_out", pinLogic(L)}}),
        TruthRow({{"A", pinLogic(H)}, {"B", pinLogic(L)}, {"Carry_in", pinLogic(H)}}, {{"Sum", pinLogic(L)}, {"Carry_out", pinLogic(H)}}),
        TruthRow({{"A", pinLogic(H)}, {"B", pinLogic(H)}, {"Carry_in", pinLogic(L)}}, {{"Sum", pinLogic(L)}, {"Carry_out", pinLogic(H)}}),
        TruthRow({{"A", pinLogic(H)}, {"B", pinLogic(H)}, {"Carry_in", pinLogic(H)}}, {{"Sum", pinLogic(H)}, {"Carry_out", pinLogic(H)}}),
    };
}

std::string Logic8Test::getTestName() const {
    return "Logic8Test";
}

void Logic8Test::verifyResults() {
    expect(runRows<AND8>({
        {{{"A", bits(0xF0)}, {"B", bits(0x3C)}}, {{"OUT", bits(0x30)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0x00)}}, {{"OUT", bits(0x00)}}},
    }), "Logic8Test AND8");
    expect(runRows<OR8>({
        {{{"A", bits(0xF0)}, {"B", bits(0x0F)}}, {{"OUT", bits(0xFF)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x00)}}, {{"OUT", bits(0x00)}}},
    }), "Logic8Test OR8");
    expect(runRows<XOR8>({
        {{{"A", bits(0xAA)}, {"B", bits(0x55)}}, {{"OUT", bits(0xFF)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0xFF)}}, {{"OUT", bits(0x00)}}},
    }), "Logic8Test XOR8");
    expect(runRows<NOT8>({
        {{{"A", bits(0x00)}}, {{"OUT", bits(0xFF)}}},
        {{{"A", bits(0xA5)}}, {{"OUT", bits(0x5A)}}},
    }), "Logic8Test NOT8");
    expect(runRows<NAND8>({
        {{{"A", bits(0xFF)}, {"B", bits(0x0F)}}, {{"OUT", bits(0xF0)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0xFF)}}, {{"OUT", bits(0x00)}}},
    }), "Logic8Test NAND8");
    expect(runRows<NOR8>({
        {{{"A", bits(0xF0)}, {"B", bits(0x0F)}}, {{"OUT", bits(0x00)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x00)}}, {{"OUT", bits(0xFF)}}},
    }), "Logic8Test NOR8");
}

std::string MuxTest::getTestName() const {
    return "MuxTest";
}

void MuxTest::verifyResults() {
    expect(runRows<Mux2to1>(oneBitMuxRows(2)), "MuxTest Mux2to1");
    expect(runRows<Mux4to1>(oneBitMuxRows(4)), "MuxTest Mux4to1");
    expect(runRows<Mux8to1>(oneBitMuxRows(8)), "MuxTest Mux8to1");
    expect(runRows<Mux16to1>(oneBitMuxRows(16)), "MuxTest Mux16to1");
    expect(runRows<Mux2to1_8bit>(busMuxRows(2)), "MuxTest Mux2to1_8bit");
    expect(runRows<Mux4to1_8bit>(busMuxRows(4)), "MuxTest Mux4to1_8bit");
    expect(runRows<Mux8to1_8bit>(busMuxRows(8)), "MuxTest Mux8to1_8bit");
    expect(runRows<Mux16to1_8bit>(busMuxRows(16)), "MuxTest Mux16to1_8bit");
}

std::string Arithmetic8Test::getTestName() const {
    return "Arithmetic8Test";
}

void Arithmetic8Test::verifyResults() {
    expect(runRows<TwosComplement8>({
        {{{"A", bits(0x00)}}, {{"Result", bits(0x00)}, {"Cout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x01)}}, {{"Result", bits(0xFF)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x7F)}}, {{"Result", bits(0x81)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0x80)}, {"Cout", bit(true)}, {"Overflow", bit(true)}}},
    }), "Arithmetic8Test TwosComplement8");
    expect(runRows<Subtractor8>({
        {{{"A", bits(0x05)}, {"B", bits(0x03)}}, {{"Result", bits(0x02)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x01)}}, {{"Result", bits(0xFF)}, {"Cout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x80)}, {"B", bits(0x01)}}, {{"Result", bits(0x7F)}, {"Cout", bit(true)}, {"Overflow", bit(true)}}},
        {{{"A", bits(0x7F)}, {"B", bits(0xFF)}}, {{"Result", bits(0x80)}, {"Cout", bit(false)}, {"Overflow", bit(true)}}},
    }), "Arithmetic8Test Subtractor8");
    expect(runRows<SubtractorWithBorrow8>({
        {{{"A", bits(0x05)}, {"B", bits(0x03)}, {"Bin", bit(false)}}, {{"Result", bits(0x02)}, {"Bout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x05)}, {"B", bits(0x03)}, {"Bin", bit(true)}}, {{"Result", bits(0x01)}, {"Bout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x00)}, {"Bin", bit(true)}}, {{"Result", bits(0xFF)}, {"Bout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x80)}, {"B", bits(0x00)}, {"Bin", bit(true)}}, {{"Result", bits(0x7F)}, {"Bout", bit(true)}, {"Overflow", bit(true)}}},
    }), "Arithmetic8Test SubtractorWithBorrow8");
    expect(runRows<Incrementer8>({
        {{{"A", bits(0x00)}}, {{"Result", bits(0x01)}, {"Cout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x7F)}}, {{"Result", bits(0x80)}, {"Cout", bit(false)}, {"Overflow", bit(true)}}},
        {{{"A", bits(0xFF)}}, {{"Result", bits(0x00)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
    }), "Arithmetic8Test Incrementer8");
    expect(runRows<Decrementer8>({
        {{{"A", bits(0x00)}}, {{"Result", bits(0xFF)}, {"Bout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x01)}}, {{"Result", bits(0x00)}, {"Bout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0x7F)}, {"Bout", bit(true)}, {"Overflow", bit(true)}}},
    }), "Arithmetic8Test Decrementer8");
}

std::string Comparator8Test::getTestName() const {
    return "Comparator8Test";
}

void Comparator8Test::verifyResults() {
    expect(runRows<EqualityChecker8>({
        {{{"A", bits(0x42)}, {"B", bits(0x42)}}, {{"EQ", bit(true)}}},
        {{{"A", bits(0x42)}, {"B", bits(0x43)}}, {{"EQ", bit(false)}}},
    }), "Comparator8Test EqualityChecker8");
    expect(runRows<Comparator8>({
        {{{"A", bits(0x01)}, {"B", bits(0x02)}}, {{"LT", bit(true)}, {"GT", bit(false)}, {"EQ", bit(false)}}},
        {{{"A", bits(0x42)}, {"B", bits(0x42)}}, {{"LT", bit(false)}, {"GT", bit(false)}, {"EQ", bit(true)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0x02)}}, {{"LT", bit(false)}, {"GT", bit(true)}, {"EQ", bit(false)}}},
    }), "Comparator8Test Comparator8");
    expect(runRows<SignedComparator8>({
        {{{"A", bits(0xFF)}, {"B", bits(0x01)}}, {{"SLT", bit(true)}, {"SGT", bit(false)}, {"SEQ", bit(false)}}},
        {{{"A", bits(0x7F)}, {"B", bits(0x80)}}, {{"SLT", bit(false)}, {"SGT", bit(true)}, {"SEQ", bit(false)}}},
        {{{"A", bits(0x80)}, {"B", bits(0x80)}}, {{"SLT", bit(false)}, {"SGT", bit(false)}, {"SEQ", bit(true)}}},
    }), "Comparator8Test SignedComparator8");
}

std::string Shifter8Test::getTestName() const {
    return "Shifter8Test";
}

void Shifter8Test::verifyResults() {
    expect(runRows<ShiftLeftLogical8>({
        {{{"A", bits(0x01)}}, {{"Result", bits(0x02)}, {"Carry", bit(false)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0x00)}, {"Carry", bit(true)}}},
    }), "Shifter8Test ShiftLeftLogical8");
    expect(runRows<ShiftRightLogical8>({
        {{{"A", bits(0x81)}}, {{"Result", bits(0x40)}, {"Carry", bit(true)}}},
        {{{"A", bits(0x02)}}, {{"Result", bits(0x01)}, {"Carry", bit(false)}}},
    }), "Shifter8Test ShiftRightLogical8");
    expect(runRows<ShiftRightArithmetic8>({
        {{{"A", bits(0x81)}}, {{"Result", bits(0xC0)}, {"Carry", bit(true)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0xC0)}, {"Carry", bit(false)}}},
        {{{"A", bits(0x02)}}, {{"Result", bits(0x01)}, {"Carry", bit(false)}}},
    }), "Shifter8Test ShiftRightArithmetic8");
}

Adder8Test::Adder8Test()
    : ComponentTruthTableTest<Adder8>("Adder8Test", "ADDER8_ROOT") {
    time_step_ = 50;
}

std::vector<TruthRow> Adder8Test::getTruthTable() const {
    return {
        adderRow(0x00, 0x00, false, 0x00, false),
        adderRow(0x01, 0x02, false, 0x03, false),
        adderRow(0x0F, 0x01, false, 0x10, false),
        adderRow(0xFF, 0x01, false, 0x00, true),
        adderRow(0xFF, 0x00, true, 0x00, true),
        adderRow(0x7F, 0x00, true, 0x80, false),
        adderRow(0x80, 0x80, false, 0x00, true),
        adderRow(0xAA, 0x55, true, 0x00, true),
    };
}

ZeroDetect8Test::ZeroDetect8Test()
    : ComponentTruthTableTest<ZeroDetect8>("ZeroDetect8Test", "ZERO_DETECT8_ROOT") {
    time_step_ = 20;
}

std::vector<TruthRow> ZeroDetect8Test::getTruthTable() const {
    return {
        TruthRow({{"A", pinBits(0x00)}}, {{"ZERO", pinBit(true)}}),
        TruthRow({{"A", pinBits(0x01)}}, {{"ZERO", pinBit(false)}}),
        TruthRow({{"A", pinBits(0x10)}}, {{"ZERO", pinBit(false)}}),
        TruthRow({{"A", pinBits(0x80)}}, {{"ZERO", pinBit(false)}}),
        TruthRow({{"A", pinBits(0xFF)}}, {{"ZERO", pinBit(false)}}),
    };
}

ALU8Test::ALU8Test()
    : ComponentTruthTableTest<ALU8>("ALU8Test", "ALU8_ROOT") {
    time_step_ = 1000;
}

std::vector<TruthRow> ALU8Test::getTruthTable() const {
    return {
        aluRow(0x00, 0x00, 0x0, 0x00, true, false, false, false),
        aluRow(0xFF, 0x01, 0x0, 0x00, true, true, false, false),
        aluRow(0x7F, 0x01, 0x0, 0x80, false, false, true, true),
        aluRow(0x05, 0x03, 0x1, 0x02, false, true, false, false),
        aluRow(0x00, 0x01, 0x1, 0xFF, false, false, false, true),
        aluRow(0xFF, 0x0F, 0x2, 0x0F, false, false, false, false),
        aluRow(0xF0, 0x0F, 0x3, 0xFF, false, false, false, true),
        aluRow(0xAA, 0x55, 0x4, 0xFF, false, false, false, true),
        aluRow(0x00, 0x00, 0x5, 0xFF, false, false, false, true),
        aluRow(0x80, 0x00, 0x6, 0x00, true, true, false, false),
        aluRow(0x01, 0x00, 0x6, 0x02, false, false, false, false),
        aluRow(0x01, 0x00, 0x7, 0x00, true, true, false, false),
        aluRow(0x02, 0x00, 0x7, 0x01, false, false, false, false),
        aluRow(0x80, 0x00, 0x8, 0xC0, false, false, false, true),
        aluRow(0xFF, 0x00, 0x9, 0x00, true, true, false, false),
        aluRow(0x7F, 0x00, 0x9, 0x80, false, false, true, true),
        aluRow(0x00, 0x00, 0xA, 0xFF, false, false, false, true),
        aluRow(0x80, 0x00, 0xA, 0x7F, false, true, true, false),
        aluRow(0x01, 0x00, 0xB, 0xFF, false, true, false, true),
        aluRow(0x80, 0x00, 0xB, 0x80, false, true, true, true),
        aluRow(0x42, 0xFF, 0xC, 0x42, false, false, false, false),
        aluRow(0xFF, 0x42, 0xD, 0x42, false, false, false, false),
        aluRow(0x05, 0x03, 0xE, 0x02, false, true, false, false),
        aluRow(0xFF, 0xFF, 0xF, 0x00, true, false, false, false),
    };
}
