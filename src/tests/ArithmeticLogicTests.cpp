#include "tests/ArithmeticLogicTests.hpp"

#include "modules/basic/Decoder.hpp"
#include "modules/basic/Logic8.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Arithmetic8.hpp"
#include "modules/composite/Comparator8.hpp"
#include "modules/composite/Shifter8.hpp"
#include "tests/TestHelpers.hpp"
#include <array>
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

TestValue pinBits(uint64_t value) {
    return bits(value);
}

TestValue pinBit(bool value) {
    return bit(value);
}

TestValue pinLogic(LogicValue value) {
    return TestValue(value);
}

TestValue pinVector(std::vector<LogicValue> values) {
    return TestValue(std::move(values));
}

enum class LogicOperation {
    And,
    Or,
    Xor,
    Nand,
    Nor,
};

LogicValue invertKnown(LogicValue value) {
    if (value == LogicValue::LOW) {
        return LogicValue::HIGH;
    }
    if (value == LogicValue::HIGH) {
        return LogicValue::LOW;
    }
    return LogicValue::UNKNOWN;
}

LogicValue logicExpected(
    LogicOperation operation,
    LogicValue left,
    LogicValue right) {
    LogicValue result = LogicValue::UNKNOWN;
    if (operation == LogicOperation::And
        || operation == LogicOperation::Nand) {
        if (left == LogicValue::LOW || right == LogicValue::LOW) {
            result = LogicValue::LOW;
        } else if (
            left == LogicValue::HIGH && right == LogicValue::HIGH) {
            result = LogicValue::HIGH;
        }
        return operation == LogicOperation::Nand
            ? invertKnown(result)
            : result;
    }
    if (operation == LogicOperation::Or
        || operation == LogicOperation::Nor) {
        if (left == LogicValue::HIGH || right == LogicValue::HIGH) {
            result = LogicValue::HIGH;
        } else if (
            left == LogicValue::LOW && right == LogicValue::LOW) {
            result = LogicValue::LOW;
        }
        return operation == LogicOperation::Nor
            ? invertKnown(result)
            : result;
    }
    if ((left == LogicValue::LOW || left == LogicValue::HIGH)
        && (right == LogicValue::LOW || right == LogicValue::HIGH)) {
        return left == right ? LogicValue::LOW : LogicValue::HIGH;
    }
    return LogicValue::UNKNOWN;
}

std::vector<LogicValue> logicVectorExpected(
    LogicOperation operation,
    const std::vector<LogicValue>& left,
    const std::vector<LogicValue>& right) {
    assert(left.size() == right.size());
    std::vector<LogicValue> result;
    result.reserve(left.size());
    for (size_t index = 0; index < left.size(); ++index) {
        result.push_back(logicExpected(
            operation, left[index], right[index]));
    }
    return result;
}

uint8_t knownLogicExpected(
    LogicOperation operation,
    uint8_t left,
    uint8_t right) {
    switch (operation) {
        case LogicOperation::And: return left & right;
        case LogicOperation::Or: return left | right;
        case LogicOperation::Xor: return left ^ right;
        case LogicOperation::Nand:
            return static_cast<uint8_t>(~(left & right));
        case LogicOperation::Nor:
            return static_cast<uint8_t>(~(left | right));
    }
    return 0;
}

std::vector<TestRow> binaryLogic8Rows(
    LogicOperation operation,
    std::initializer_list<std::pair<uint8_t, uint8_t>> known_cases) {
    std::vector<TestRow> rows;
    for (const auto [left, right] : known_cases) {
        rows.push_back({
            {{"A", bits(left)}, {"B", bits(right)}},
            {{"OUT", bits(knownLogicExpected(
                operation, left, right))}},
        });
    }
    const std::vector<LogicValue> left{
        LogicValue::LOW,
        LogicValue::HIGH,
        LogicValue::UNKNOWN,
        LogicValue::HIGH_Z,
        LogicValue::LOW,
        LogicValue::HIGH,
        LogicValue::UNKNOWN,
        LogicValue::HIGH_Z,
    };
    const std::vector<LogicValue> right{
        LogicValue::LOW,
        LogicValue::LOW,
        LogicValue::LOW,
        LogicValue::LOW,
        LogicValue::HIGH,
        LogicValue::HIGH,
        LogicValue::HIGH,
        LogicValue::HIGH,
    };
    rows.push_back({
        {{"A", pinVector(left)}, {"B", pinVector(right)}},
        {{"OUT", pinVector(logicVectorExpected(
            operation, left, right))}},
    });
    return rows;
}

std::vector<TestRow> not8Rows() {
    std::vector<TestRow> rows{
        {{{"A", bits(0x00)}}, {{"OUT", bits(0xFF)}}},
        {{{"A", bits(0xA5)}}, {{"OUT", bits(0x5A)}}},
        {{{"A", bits(0xFF)}}, {{"OUT", bits(0x00)}}},
        {{{"A", bits(0x81)}}, {{"OUT", bits(0x7E)}}},
    };
    const std::vector<LogicValue> input{
        LogicValue::LOW,
        LogicValue::HIGH,
        LogicValue::UNKNOWN,
        LogicValue::HIGH_Z,
        LogicValue::HIGH_Z,
        LogicValue::UNKNOWN,
        LogicValue::HIGH,
        LogicValue::LOW,
    };
    std::vector<LogicValue> output;
    output.reserve(input.size());
    for (const auto value : input) {
        output.push_back(invertKnown(value));
    }
    rows.push_back({
        {{"A", pinVector(input)}},
        {{"OUT", pinVector(output)}},
    });
    return rows;
}

TestRow adderRow(uint64_t a, uint64_t b, bool cin, uint64_t sum, bool cout) {
    return {{{"A", pinBits(a)}, {"B", pinBits(b)}, {"Cin", pinBit(cin)}},
            {{"Sum", pinBits(sum)}, {"Cout", pinBit(cout)}}};
}

TestRow aluRow(uint64_t a, uint64_t b, uint64_t op, uint64_t out, bool zero, bool carry, bool overflow, bool negative) {
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

    const auto selected = input_count / 2;
    const auto unselected = (selected + 1) % input_count;
    for (const auto selected_value :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        std::map<std::string, TestValue> inputs;
        for (size_t i = 0; i < input_count; ++i) {
            inputs[muxInputName(input_count, i)] = bit(false);
        }
        inputs[muxInputName(input_count, selected)] =
            pinLogic(selected_value);
        inputs["SEL"] = bits(selected);
        rows.push_back({
            std::move(inputs),
            {{"OUT", pinLogic(LogicValue::UNKNOWN)}},
        });
    }
    for (const auto unselected_value :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        std::map<std::string, TestValue> inputs;
        for (size_t i = 0; i < input_count; ++i) {
            inputs[muxInputName(input_count, i)] = bit(false);
        }
        inputs[muxInputName(input_count, selected)] = bit(true);
        inputs[muxInputName(input_count, unselected)] =
            pinLogic(unselected_value);
        inputs["SEL"] = bits(selected);
        rows.push_back({std::move(inputs), {{"OUT", bit(true)}}});
    }

    size_t select_width = 0;
    for (auto count = input_count; count > 1; count >>= 1U) {
        ++select_width;
    }
    for (const auto unknown_select :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        std::map<std::string, TestValue> inputs;
        for (size_t i = 0; i < input_count; ++i) {
            inputs[muxInputName(input_count, i)] = bit(false);
        }
        inputs[muxInputName(input_count, 1)] = bit(true);
        auto selector =
            std::vector<LogicValue>(select_width, LogicValue::LOW);
        selector[0] = unknown_select;
        inputs["SEL"] = pinVector(std::move(selector));
        rows.push_back({
            std::move(inputs),
            {{"OUT", pinLogic(LogicValue::UNKNOWN)}},
        });
    }
    return rows;
}

uint64_t muxBusValue(size_t index) {
    return ((index * 0x11U) + 0x13U) & 0xFFU;
}

uint64_t mux32BusValue(size_t index) {
    return (0x10203040ULL + index * 0x11111111ULL) & 0xffffffffULL;
}

void appendBusMuxFourStateRows(
    std::vector<TestRow>& rows,
    size_t input_count,
    size_t bus_width,
    uint64_t known_selected_value) {
    const auto selected = input_count / 2;
    const auto unselected = (selected + 1) % input_count;
    std::vector<LogicValue> mixed;
    mixed.reserve(bus_width);
    constexpr std::array<LogicValue, 4> Values{
        LogicValue::LOW,
        LogicValue::HIGH,
        LogicValue::UNKNOWN,
        LogicValue::HIGH_Z,
    };
    for (size_t bit_index = 0; bit_index < bus_width; ++bit_index) {
        mixed.push_back(Values[bit_index % Values.size()]);
    }
    auto selected_expected = mixed;
    for (auto& value : selected_expected) {
        if (value == LogicValue::HIGH_Z) {
            value = LogicValue::UNKNOWN;
        }
    }

    std::map<std::string, TestValue> selected_inputs;
    for (size_t i = 0; i < input_count; ++i) {
        selected_inputs[muxInputName(input_count, i)] = bits(0);
    }
    selected_inputs[muxInputName(input_count, selected)] =
        pinVector(mixed);
    selected_inputs["SEL"] = bits(selected);
    rows.push_back({
        std::move(selected_inputs),
        {{"OUT", pinVector(selected_expected)}},
    });

    std::map<std::string, TestValue> masked_inputs;
    for (size_t i = 0; i < input_count; ++i) {
        masked_inputs[muxInputName(input_count, i)] = bits(0);
    }
    masked_inputs[muxInputName(input_count, selected)] =
        bits(known_selected_value);
    masked_inputs[muxInputName(input_count, unselected)] =
        pinVector(mixed);
    masked_inputs["SEL"] = bits(selected);
    rows.push_back({
        std::move(masked_inputs),
        {{"OUT", bits(known_selected_value)}},
    });

    size_t select_width = 0;
    for (auto count = input_count; count > 1; count >>= 1U) {
        ++select_width;
    }
    for (const auto unknown_select :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        std::map<std::string, TestValue> inputs;
        for (size_t i = 0; i < input_count; ++i) {
            inputs[muxInputName(input_count, i)] = bits(0);
        }
        const auto mask = bus_width == 32
            ? 0xffffffffULL
            : ((uint64_t{1} << bus_width) - 1U);
        inputs[muxInputName(input_count, 1)] = bits(mask);
        auto selector =
            std::vector<LogicValue>(select_width, LogicValue::LOW);
        selector[0] = unknown_select;
        inputs["SEL"] = pinVector(std::move(selector));
        rows.push_back({
            std::move(inputs),
            {{"OUT", pinVector(std::vector<LogicValue>(
                bus_width, LogicValue::UNKNOWN))}},
        });
    }
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
    appendBusMuxFourStateRows(rows, input_count, 8, 0xa5);
    return rows;
}

std::vector<TestRow> busMux4Rows(size_t input_count) {
    std::vector<TestRow> rows;
    for (size_t selected = 0; selected < input_count; ++selected) {
        std::map<std::string, TestValue> inputs;
        for (size_t i = 0; i < input_count; ++i) {
            inputs[muxInputName(input_count, i)] = bits((i * 0x5U + 0x3U) & 0xFU);
        }
        inputs["SEL"] = bits(selected);
        rows.push_back({inputs, {{"OUT", bits((selected * 0x5U + 0x3U) & 0xFU)}}});
    }
    appendBusMuxFourStateRows(rows, input_count, 4, 0x0a);
    return rows;
}

std::vector<TestRow> busMux32Rows(size_t input_count) {
    std::vector<TestRow> rows;
    for (size_t selected = 0; selected < input_count; ++selected) {
        std::map<std::string, TestValue> inputs;
        for (size_t i = 0; i < input_count; ++i) {
            inputs[muxInputName(input_count, i)] = bits(mux32BusValue(i));
        }
        inputs["SEL"] = bits(selected);
        rows.push_back({inputs, {{"OUT", bits(mux32BusValue(selected))}}});
    }
    appendBusMuxFourStateRows(
        rows, input_count, 32, 0xa5a55a5aU);
    return rows;
}

std::map<std::string, TestValue> decoderOutputs(size_t output_count, size_t selected, bool enable) {
    std::map<std::string, TestValue> outputs;
    for (size_t i = 0; i < output_count; ++i) {
        outputs["OUT" + std::to_string(i)] = bit(enable && i == selected);
    }
    return outputs;
}

std::vector<TestRow> decoderRows(size_t output_count) {
    std::vector<TestRow> rows;
    for (size_t selected = 0; selected < output_count; ++selected) {
        rows.push_back({
            {{"ADDR", bits(selected)}, {"ENABLE", bit(true)}},
            decoderOutputs(output_count, selected, true),
        });
    }
    for (size_t selected : {size_t{0}, output_count / 2, output_count - 1}) {
        rows.push_back({
            {{"ADDR", bits(selected)}, {"ENABLE", bit(false)}},
            decoderOutputs(output_count, selected, false),
        });
    }
    size_t address_width = 0;
    for (auto count = output_count; count > 1; count >>= 1U) {
        ++address_width;
    }
    const auto all_unknown =
        std::vector<LogicValue>(address_width, LogicValue::UNKNOWN);
    const auto all_high_z =
        std::vector<LogicValue>(address_width, LogicValue::HIGH_Z);
    rows.push_back({
        {{"ADDR", pinVector(all_unknown)}, {"ENABLE", bit(false)}},
        decoderOutputs(output_count, 0, false),
    });
    for (const auto& address : {all_unknown, all_high_z}) {
        std::map<std::string, TestValue> outputs;
        for (size_t i = 0; i < output_count; ++i) {
            outputs["OUT" + std::to_string(i)] =
                pinLogic(LogicValue::UNKNOWN);
        }
        rows.push_back({
            {{"ADDR", pinVector(address)}, {"ENABLE", bit(true)}},
            std::move(outputs),
        });
    }
    for (const auto enable :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        const auto selected = output_count / 2;
        auto outputs = decoderOutputs(output_count, selected, false);
        outputs["OUT" + std::to_string(selected)] =
            pinLogic(LogicValue::UNKNOWN);
        rows.push_back({
            {
                {"ADDR", bits(selected)},
                {"ENABLE", pinLogic(enable)},
            },
            std::move(outputs),
        });
    }
    return rows;
}

std::vector<LogicValue> bus8With(
    size_t bit_index,
    LogicValue value,
    uint8_t known_value = 0) {
    auto result = circuit::test::logicBits(8, known_value);
    result.at(bit_index) = value;
    return result;
}

TestRow twosComplementFourStateRow() {
    const auto input = bus8With(0, LogicValue::UNKNOWN);
    return {
        {{"A", pinVector(input)}},
        {
            {"Result", pinVector(std::vector<LogicValue>(
                8, LogicValue::UNKNOWN))},
            {"Cout", pinLogic(LogicValue::UNKNOWN)},
            {"Overflow", bit(false)},
        },
    };
}

TestRow subtractorFourStateRow() {
    const auto right = bus8With(0, LogicValue::HIGH_Z);
    return {
        {{"A", bits(0)}, {"B", pinVector(right)}},
        {
            {"Result", pinVector(std::vector<LogicValue>(
                8, LogicValue::UNKNOWN))},
            {"Cout", pinLogic(LogicValue::UNKNOWN)},
            {"Overflow", bit(false)},
        },
    };
}

TestRow subtractorBorrowFourStateRow() {
    return {
        {
            {"A", bits(0)},
            {"B", bits(0)},
            {"Bin", pinLogic(LogicValue::UNKNOWN)},
        },
        {
            {"Result", pinVector(std::vector<LogicValue>(
                8, LogicValue::UNKNOWN))},
            {"Bout", pinLogic(LogicValue::UNKNOWN)},
            {"Overflow", bit(false)},
        },
    };
}

TestRow incrementerFourStateRow() {
    const auto input = bus8With(0, LogicValue::UNKNOWN);
    auto result = circuit::test::logicBits(8, 0);
    result[0] = LogicValue::UNKNOWN;
    result[1] = LogicValue::UNKNOWN;
    return {
        {{"A", pinVector(input)}},
        {
            {"Result", pinVector(result)},
            {"Cout", bit(false)},
            {"Overflow", bit(false)},
        },
    };
}

TestRow decrementerFourStateRow() {
    const auto input = bus8With(0, LogicValue::HIGH_Z);
    return {
        {{"A", pinVector(input)}},
        {
            {"Result", pinVector(std::vector<LogicValue>(
                8, LogicValue::UNKNOWN))},
            {"Bout", pinLogic(LogicValue::UNKNOWN)},
            {"Overflow", bit(false)},
        },
    };
}

TestRow equalityFourStateRow() {
    const auto right = bus8With(0, LogicValue::UNKNOWN);
    return {
        {{"A", bits(0)}, {"B", pinVector(right)}},
        {{"EQ", pinLogic(LogicValue::UNKNOWN)}},
    };
}

TestRow comparatorFourStateRow() {
    const auto right = bus8With(0, LogicValue::HIGH_Z);
    return {
        {{"A", bits(0)}, {"B", pinVector(right)}},
        {
            {"LT", pinLogic(LogicValue::UNKNOWN)},
            {"GT", pinLogic(LogicValue::UNKNOWN)},
            {"EQ", pinLogic(LogicValue::UNKNOWN)},
        },
    };
}

TestRow signedComparatorFourStateRow() {
    const auto right = bus8With(0, LogicValue::UNKNOWN);
    return {
        {{"A", bits(0)}, {"B", pinVector(right)}},
        {
            {"SLT", pinLogic(LogicValue::UNKNOWN)},
            {"SGT", pinLogic(LogicValue::UNKNOWN)},
            {"SEQ", pinLogic(LogicValue::UNKNOWN)},
        },
    };
}

TestRow shiftLeftFourStateRow() {
    const auto input = bus8With(7, LogicValue::HIGH_Z, 0x01);
    auto result = circuit::test::logicBits(8, 0x02);
    return {
        {{"A", pinVector(input)}},
        {
            {"Result", pinVector(result)},
            {"Carry", pinLogic(LogicValue::HIGH_Z)},
        },
    };
}

TestRow shiftRightLogicalFourStateRow() {
    const auto input = bus8With(0, LogicValue::UNKNOWN, 0x80);
    auto result = circuit::test::logicBits(8, 0x40);
    return {
        {{"A", pinVector(input)}},
        {
            {"Result", pinVector(result)},
            {"Carry", pinLogic(LogicValue::UNKNOWN)},
        },
    };
}

TestRow shiftRightArithmeticFourStateRow() {
    const auto input = bus8With(7, LogicValue::HIGH_Z, 0x01);
    auto result = circuit::test::logicBits(8, 0);
    result[6] = LogicValue::HIGH_Z;
    result[7] = LogicValue::HIGH_Z;
    return {
        {{"A", pinVector(input)}},
        {
            {"Result", pinVector(result)},
            {"Carry", bit(true)},
        },
    };
}
}

std::vector<TestRow> halfAdderRows();
std::vector<TestRow> fullAdderRows();
std::vector<TestRow> adder8Rows();
std::vector<TestRow> zeroDetect8Rows();
std::vector<TestRow> alu8Rows();

HalfAdderTest::HalfAdderTest()
    : TruthTableComponentTest<HalfAdder>(
          "HalfAdderTest", "HA_ROOT", halfAdderRows()) {}

std::vector<TestRow> halfAdderRows() {
    constexpr std::array<LogicValue, 4> Values{
        LogicValue::LOW,
        LogicValue::HIGH,
        LogicValue::UNKNOWN,
        LogicValue::HIGH_Z,
    };
    std::vector<TestRow> rows;
    for (const auto left : Values) {
        for (const auto right : Values) {
            rows.push_back({
                {{"A", pinLogic(left)}, {"B", pinLogic(right)}},
                {
                    {"Sum", pinLogic(logicExpected(
                        LogicOperation::Xor, left, right))},
                    {"Carry", pinLogic(logicExpected(
                        LogicOperation::And, left, right))},
                },
            });
        }
    }
    return rows;
}

FullAdderTest::FullAdderTest()
    : TruthTableComponentTest<FullAdder>(
          "FullAdderTest", "FA_ROOT", fullAdderRows()) {}

std::vector<TestRow> fullAdderRows() {
    constexpr std::array<LogicValue, 4> Values{
        LogicValue::LOW,
        LogicValue::HIGH,
        LogicValue::UNKNOWN,
        LogicValue::HIGH_Z,
    };
    std::vector<TestRow> rows;
    for (const auto left : Values) {
        for (const auto right : Values) {
            for (const auto carry_in : Values) {
                const auto first_sum = logicExpected(
                    LogicOperation::Xor, left, right);
                const auto first_carry = logicExpected(
                    LogicOperation::And, left, right);
                const auto second_carry = logicExpected(
                    LogicOperation::And, first_sum, carry_in);
                rows.push_back({
                    {
                        {"A", pinLogic(left)},
                        {"B", pinLogic(right)},
                        {"Carry_in", pinLogic(carry_in)},
                    },
                    {
                        {"Sum", pinLogic(logicExpected(
                            LogicOperation::Xor,
                            first_sum,
                            carry_in))},
                        {"Carry_out", pinLogic(logicExpected(
                            LogicOperation::Or,
                            first_carry,
                            second_carry))},
                    },
                });
            }
        }
    }
    return rows;
}

AND8Test::AND8Test()
    : TruthTableComponentTest<AND8>(
          "AND8Test",
          "AND8_ROOT",
          binaryLogic8Rows(
              LogicOperation::And,
              {{0xF0, 0x3C}, {0xFF, 0x00}, {0xA5, 0x5A}, {0x81, 0xC3}})) {}

OR8Test::OR8Test()
    : TruthTableComponentTest<OR8>(
          "OR8Test",
          "OR8_ROOT",
          binaryLogic8Rows(
              LogicOperation::Or,
              {{0xF0, 0x0F}, {0x00, 0x00}, {0xA5, 0x5A}, {0x81, 0x42}})) {}

XOR8Test::XOR8Test()
    : TruthTableComponentTest<XOR8>(
          "XOR8Test",
          "XOR8_ROOT",
          binaryLogic8Rows(
              LogicOperation::Xor,
              {{0xAA, 0x55}, {0xFF, 0xFF}, {0xF0, 0x3C}, {0x81, 0x42}})) {}

NOT8Test::NOT8Test()
    : TruthTableComponentTest<NOT8>(
          "NOT8Test", "NOT8_ROOT", not8Rows()) {}

NAND8Test::NAND8Test()
    : TruthTableComponentTest<NAND8>(
          "NAND8Test",
          "NAND8_ROOT",
          binaryLogic8Rows(
              LogicOperation::Nand,
              {{0xFF, 0x0F}, {0xFF, 0xFF}, {0x00, 0xFF}, {0x81, 0xC3}})) {}

NOR8Test::NOR8Test()
    : TruthTableComponentTest<NOR8>(
          "NOR8Test",
          "NOR8_ROOT",
          binaryLogic8Rows(
              LogicOperation::Nor,
              {{0xF0, 0x0F}, {0x00, 0x00}, {0xA5, 0x5A}, {0x81, 0x42}})) {}

Mux2to1Test::Mux2to1Test()
    : TruthTableComponentTest<Mux2to1>("Mux2to1Test", "MUX2TO1_ROOT", oneBitMuxRows(2)) {}

Mux4to1Test::Mux4to1Test()
    : TruthTableComponentTest<Mux4to1>("Mux4to1Test", "MUX4TO1_ROOT", oneBitMuxRows(4)) {}

Mux8to1Test::Mux8to1Test()
    : TruthTableComponentTest<Mux8to1>("Mux8to1Test", "MUX8TO1_ROOT", oneBitMuxRows(8)) {}

Mux16to1Test::Mux16to1Test()
    : TruthTableComponentTest<Mux16to1>("Mux16to1Test", "MUX16TO1_ROOT", oneBitMuxRows(16)) {}

Mux32to1Test::Mux32to1Test()
    : TruthTableComponentTest<Mux32to1>("Mux32to1Test", "MUX32TO1_ROOT", oneBitMuxRows(32)) {}

Mux2to1_8bitTest::Mux2to1_8bitTest()
    : TruthTableComponentTest<Mux2to1_8bit>("Mux2to1_8bitTest", "MUX2TO1_8BIT_ROOT", busMuxRows(2)) {}

Mux2to1_4bitTest::Mux2to1_4bitTest()
    : TruthTableComponentTest<Mux2to1_4bit>("Mux2to1_4bitTest", "MUX2TO1_4BIT_ROOT", busMux4Rows(2)) {}

Mux2to1_32bitTest::Mux2to1_32bitTest()
    : TruthTableComponentTest<Mux2to1_32bit>("Mux2to1_32bitTest", "MUX2TO1_32BIT_ROOT", busMux32Rows(2)) {}

Mux4to1_8bitTest::Mux4to1_8bitTest()
    : TruthTableComponentTest<Mux4to1_8bit>("Mux4to1_8bitTest", "MUX4TO1_8BIT_ROOT", busMuxRows(4)) {}

Mux4to1_32bitTest::Mux4to1_32bitTest()
    : TruthTableComponentTest<Mux4to1_32bit>("Mux4to1_32bitTest", "MUX4TO1_32BIT_ROOT", busMux32Rows(4)) {}

Mux8to1_8bitTest::Mux8to1_8bitTest()
    : TruthTableComponentTest<Mux8to1_8bit>("Mux8to1_8bitTest", "MUX8TO1_8BIT_ROOT", busMuxRows(8)) {}

Mux8to1_32bitTest::Mux8to1_32bitTest()
    : TruthTableComponentTest<Mux8to1_32bit>("Mux8to1_32bitTest", "MUX8TO1_32BIT_ROOT", busMux32Rows(8)) {}

Mux16to1_8bitTest::Mux16to1_8bitTest()
    : TruthTableComponentTest<Mux16to1_8bit>("Mux16to1_8bitTest", "MUX16TO1_8BIT_ROOT", busMuxRows(16)) {}

Mux32to1_32bitTest::Mux32to1_32bitTest()
    : TruthTableComponentTest<Mux32to1_32bit>("Mux32to1_32bitTest", "MUX32TO1_32BIT_ROOT", busMux32Rows(32)) {}

Decoder2to4Test::Decoder2to4Test()
    : TruthTableComponentTest<Decoder2to4>("Decoder2to4Test", "DECODER2TO4_ROOT", decoderRows(4)) {}

Decoder5to32Test::Decoder5to32Test()
    : TruthTableComponentTest<Decoder5to32>("Decoder5to32Test", "DECODER5TO32_ROOT", decoderRows(32)) {}

TwosComplement8Test::TwosComplement8Test()
    : TruthTableComponentTest<TwosComplement8>("TwosComplement8Test", "TWOS_COMPLEMENT8_ROOT", {
        {{{"A", bits(0x00)}}, {{"Result", bits(0x00)}, {"Cout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x01)}}, {{"Result", bits(0xFF)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x7F)}}, {{"Result", bits(0x81)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0x80)}, {"Cout", bit(true)}, {"Overflow", bit(true)}}},
        {{{"A", bits(0xFF)}}, {{"Result", bits(0x01)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x55)}}, {{"Result", bits(0xAB)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        twosComplementFourStateRow(),
    }) {}

Subtractor8Test::Subtractor8Test()
    : TruthTableComponentTest<Subtractor8>("Subtractor8Test", "SUBTRACTOR8_ROOT", {
        {{{"A", bits(0x05)}, {"B", bits(0x03)}}, {{"Result", bits(0x02)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x01)}}, {{"Result", bits(0xFF)}, {"Cout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x80)}, {"B", bits(0x01)}}, {{"Result", bits(0x7F)}, {"Cout", bit(true)}, {"Overflow", bit(true)}}},
        {{{"A", bits(0x7F)}, {"B", bits(0xFF)}}, {{"Result", bits(0x80)}, {"Cout", bit(false)}, {"Overflow", bit(true)}}},
        {{{"A", bits(0x10)}, {"B", bits(0x10)}}, {{"Result", bits(0x00)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0x01)}}, {{"Result", bits(0xFE)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x80)}}, {{"Result", bits(0x80)}, {"Cout", bit(false)}, {"Overflow", bit(true)}}},
        {{{"A", bits(0x80)}, {"B", bits(0x7F)}}, {{"Result", bits(0x01)}, {"Cout", bit(true)}, {"Overflow", bit(true)}}},
        subtractorFourStateRow(),
    }) {}

SubtractorWithBorrow8Test::SubtractorWithBorrow8Test()
    : TruthTableComponentTest<SubtractorWithBorrow8>("SubtractorWithBorrow8Test", "SUBTRACTOR_WITH_BORROW8_ROOT", {
        {{{"A", bits(0x05)}, {"B", bits(0x03)}, {"Bin", bit(false)}}, {{"Result", bits(0x02)}, {"Bout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x05)}, {"B", bits(0x03)}, {"Bin", bit(true)}}, {{"Result", bits(0x01)}, {"Bout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x00)}, {"Bin", bit(true)}}, {{"Result", bits(0xFF)}, {"Bout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x80)}, {"B", bits(0x00)}, {"Bin", bit(true)}}, {{"Result", bits(0x7F)}, {"Bout", bit(true)}, {"Overflow", bit(true)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x01)}, {"Bin", bit(true)}}, {{"Result", bits(0xFE)}, {"Bout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x7F)}, {"B", bits(0xFF)}, {"Bin", bit(true)}}, {{"Result", bits(0x7F)}, {"Bout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x80)}, {"B", bits(0x7F)}, {"Bin", bit(true)}}, {{"Result", bits(0x00)}, {"Bout", bit(true)}, {"Overflow", bit(true)}}},
        subtractorBorrowFourStateRow(),
    }) {}

Incrementer8Test::Incrementer8Test()
    : TruthTableComponentTest<Incrementer8>("Incrementer8Test", "INCREMENTER8_ROOT", {
        {{{"A", bits(0x00)}}, {{"Result", bits(0x01)}, {"Cout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x7F)}}, {{"Result", bits(0x80)}, {"Cout", bit(false)}, {"Overflow", bit(true)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0x81)}, {"Cout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0xFE)}}, {{"Result", bits(0xFF)}, {"Cout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0xFF)}}, {{"Result", bits(0x00)}, {"Cout", bit(true)}, {"Overflow", bit(false)}}},
        incrementerFourStateRow(),
    }) {}

Decrementer8Test::Decrementer8Test()
    : TruthTableComponentTest<Decrementer8>("Decrementer8Test", "DECREMENTER8_ROOT", {
        {{{"A", bits(0x00)}}, {{"Result", bits(0xFF)}, {"Bout", bit(false)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x01)}}, {{"Result", bits(0x00)}, {"Bout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x7F)}}, {{"Result", bits(0x7E)}, {"Bout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0x7F)}, {"Bout", bit(true)}, {"Overflow", bit(true)}}},
        {{{"A", bits(0x81)}}, {{"Result", bits(0x80)}, {"Bout", bit(true)}, {"Overflow", bit(false)}}},
        {{{"A", bits(0xFF)}}, {{"Result", bits(0xFE)}, {"Bout", bit(true)}, {"Overflow", bit(false)}}},
        decrementerFourStateRow(),
    }) {}

EqualityChecker8Test::EqualityChecker8Test()
    : TruthTableComponentTest<EqualityChecker8>("EqualityChecker8Test", "EQUALITY_CHECKER8_ROOT", {
        {{{"A", bits(0x42)}, {"B", bits(0x42)}}, {{"EQ", bit(true)}}},
        {{{"A", bits(0x42)}, {"B", bits(0x43)}}, {{"EQ", bit(false)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x00)}}, {{"EQ", bit(true)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0x00)}}, {{"EQ", bit(false)}}},
        {{{"A", bits(0x80)}, {"B", bits(0x00)}}, {{"EQ", bit(false)}}},
        equalityFourStateRow(),
    }) {}

Comparator8Test::Comparator8Test()
    : TruthTableComponentTest<Comparator8>("Comparator8Test", "COMPARATOR8_ROOT", {
        {{{"A", bits(0x01)}, {"B", bits(0x02)}}, {{"LT", bit(true)}, {"GT", bit(false)}, {"EQ", bit(false)}}},
        {{{"A", bits(0x42)}, {"B", bits(0x42)}}, {{"LT", bit(false)}, {"GT", bit(false)}, {"EQ", bit(true)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0x02)}}, {{"LT", bit(false)}, {"GT", bit(true)}, {"EQ", bit(false)}}},
        {{{"A", bits(0x00)}, {"B", bits(0xFF)}}, {{"LT", bit(true)}, {"GT", bit(false)}, {"EQ", bit(false)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0x00)}}, {{"LT", bit(false)}, {"GT", bit(true)}, {"EQ", bit(false)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x00)}}, {{"LT", bit(false)}, {"GT", bit(false)}, {"EQ", bit(true)}}},
        {{{"A", bits(0x80)}, {"B", bits(0x7F)}}, {{"LT", bit(false)}, {"GT", bit(true)}, {"EQ", bit(false)}}},
        comparatorFourStateRow(),
    }) {}

SignedComparator8Test::SignedComparator8Test()
    : TruthTableComponentTest<SignedComparator8>("SignedComparator8Test", "SIGNED_COMPARATOR8_ROOT", {
        {{{"A", bits(0xFF)}, {"B", bits(0x01)}}, {{"SLT", bit(true)}, {"SGT", bit(false)}, {"SEQ", bit(false)}}},
        {{{"A", bits(0x7F)}, {"B", bits(0x80)}}, {{"SLT", bit(false)}, {"SGT", bit(true)}, {"SEQ", bit(false)}}},
        {{{"A", bits(0x80)}, {"B", bits(0x80)}}, {{"SLT", bit(false)}, {"SGT", bit(false)}, {"SEQ", bit(true)}}},
        {{{"A", bits(0x80)}, {"B", bits(0xFF)}}, {{"SLT", bit(true)}, {"SGT", bit(false)}, {"SEQ", bit(false)}}},
        {{{"A", bits(0xFF)}, {"B", bits(0x80)}}, {{"SLT", bit(false)}, {"SGT", bit(true)}, {"SEQ", bit(false)}}},
        {{{"A", bits(0x00)}, {"B", bits(0xFF)}}, {{"SLT", bit(false)}, {"SGT", bit(true)}, {"SEQ", bit(false)}}},
        {{{"A", bits(0x7F)}, {"B", bits(0x01)}}, {{"SLT", bit(false)}, {"SGT", bit(true)}, {"SEQ", bit(false)}}},
        {{{"A", bits(0x00)}, {"B", bits(0x00)}}, {{"SLT", bit(false)}, {"SGT", bit(false)}, {"SEQ", bit(true)}}},
        signedComparatorFourStateRow(),
    }) {}

ShiftLeftLogical8Test::ShiftLeftLogical8Test()
    : TruthTableComponentTest<ShiftLeftLogical8>("ShiftLeftLogical8Test", "SHIFT_LEFT_LOGICAL8_ROOT", {
        {{{"A", bits(0x00)}}, {{"Result", bits(0x00)}, {"Carry", bit(false)}}},
        {{{"A", bits(0x01)}}, {{"Result", bits(0x02)}, {"Carry", bit(false)}}},
        {{{"A", bits(0x7F)}}, {{"Result", bits(0xFE)}, {"Carry", bit(false)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0x00)}, {"Carry", bit(true)}}},
        {{{"A", bits(0xFF)}}, {{"Result", bits(0xFE)}, {"Carry", bit(true)}}},
        shiftLeftFourStateRow(),
    }) {}

ShiftRightLogical8Test::ShiftRightLogical8Test()
    : TruthTableComponentTest<ShiftRightLogical8>("ShiftRightLogical8Test", "SHIFT_RIGHT_LOGICAL8_ROOT", {
        {{{"A", bits(0x00)}}, {{"Result", bits(0x00)}, {"Carry", bit(false)}}},
        {{{"A", bits(0x81)}}, {{"Result", bits(0x40)}, {"Carry", bit(true)}}},
        {{{"A", bits(0x02)}}, {{"Result", bits(0x01)}, {"Carry", bit(false)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0x40)}, {"Carry", bit(false)}}},
        {{{"A", bits(0xFF)}}, {{"Result", bits(0x7F)}, {"Carry", bit(true)}}},
        shiftRightLogicalFourStateRow(),
    }) {}

ShiftRightArithmetic8Test::ShiftRightArithmetic8Test()
    : TruthTableComponentTest<ShiftRightArithmetic8>("ShiftRightArithmetic8Test", "SHIFT_RIGHT_ARITHMETIC8_ROOT", {
        {{{"A", bits(0x00)}}, {{"Result", bits(0x00)}, {"Carry", bit(false)}}},
        {{{"A", bits(0x81)}}, {{"Result", bits(0xC0)}, {"Carry", bit(true)}}},
        {{{"A", bits(0x80)}}, {{"Result", bits(0xC0)}, {"Carry", bit(false)}}},
        {{{"A", bits(0x02)}}, {{"Result", bits(0x01)}, {"Carry", bit(false)}}},
        {{{"A", bits(0x01)}}, {{"Result", bits(0x00)}, {"Carry", bit(true)}}},
        {{{"A", bits(0x7F)}}, {{"Result", bits(0x3F)}, {"Carry", bit(true)}}},
        {{{"A", bits(0xFF)}}, {{"Result", bits(0xFF)}, {"Carry", bit(true)}}},
        shiftRightArithmeticFourStateRow(),
    }) {}

Adder8Test::Adder8Test()
    : TruthTableComponentTest<Adder8>(
          "Adder8Test", "ADDER8_ROOT", adder8Rows()) {}

std::vector<TestRow> adder8Rows() {
    std::vector<TestRow> rows{
        adderRow(0x00, 0x00, false, 0x00, false),
        adderRow(0x01, 0x02, false, 0x03, false),
        adderRow(0x0F, 0x01, false, 0x10, false),
        adderRow(0xFF, 0x01, false, 0x00, true),
        adderRow(0xFF, 0x00, true, 0x00, true),
        adderRow(0x7F, 0x00, true, 0x80, false),
        adderRow(0x80, 0x80, false, 0x00, true),
        adderRow(0xAA, 0x55, true, 0x00, true),
        adderRow(0x00, 0x00, true, 0x01, false),
        adderRow(0xFF, 0xFF, false, 0xFE, true),
        adderRow(0x80, 0x7F, true, 0x00, true),
        adderRow(0x55, 0xAA, false, 0xFF, false),
    };
    for (const auto unknown_value :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        const auto left = bus8With(0, unknown_value);
        auto expected = circuit::test::logicBits(8, 0);
        expected[0] = LogicValue::UNKNOWN;
        rows.push_back({
            {
                {"A", pinVector(left)},
                {"B", bits(0)},
                {"Cin", bit(false)},
            },
            {
                {"Sum", pinVector(expected)},
                {"Cout", bit(false)},
            },
        });
    }
    return rows;
}

ZeroDetect8Test::ZeroDetect8Test()
    : TruthTableComponentTest<ZeroDetect8>(
          "ZeroDetect8Test", "ZERO_DETECT8_ROOT", zeroDetect8Rows()) {}

std::vector<TestRow> zeroDetect8Rows() {
    std::vector<TestRow> rows{
        TestRow({{"A", pinBits(0x00)}}, {{"ZERO", pinBit(true)}}),
        TestRow({{"A", pinBits(0x01)}}, {{"ZERO", pinBit(false)}}),
        TestRow({{"A", pinBits(0x02)}}, {{"ZERO", pinBit(false)}}),
        TestRow({{"A", pinBits(0x10)}}, {{"ZERO", pinBit(false)}}),
        TestRow({{"A", pinBits(0x7F)}}, {{"ZERO", pinBit(false)}}),
        TestRow({{"A", pinBits(0x80)}}, {{"ZERO", pinBit(false)}}),
        TestRow({{"A", pinBits(0xFF)}}, {{"ZERO", pinBit(false)}}),
    };
    for (const auto unknown_value :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        const auto only_unknown = bus8With(3, unknown_value);
        rows.push_back({
            {{"A", pinVector(only_unknown)}},
            {{"ZERO", pinLogic(LogicValue::UNKNOWN)}},
        });
        const auto high_dominates =
            bus8With(3, unknown_value, 0x80);
        rows.push_back({
            {{"A", pinVector(high_dominates)}},
            {{"ZERO", bit(false)}},
        });
    }
    return rows;
}

ALU8Test::ALU8Test()
    : TruthTableComponentTest<ALU8>(
          "ALU8Test", "ALU8_ROOT", alu8Rows()) {}

std::vector<TestRow> alu8Rows() {
    std::vector<TestRow> rows{
        aluRow(0x00, 0x00, 0x0, 0x00, true, false, false, false),
        aluRow(0xFF, 0x01, 0x0, 0x00, true, true, false, false),
        aluRow(0x7F, 0x01, 0x0, 0x80, false, false, true, true),
        aluRow(0x80, 0x80, 0x0, 0x00, true, true, true, false),
        aluRow(0x05, 0x03, 0x1, 0x02, false, true, false, false),
        aluRow(0x00, 0x01, 0x1, 0xFF, false, false, false, true),
        aluRow(0x42, 0x42, 0x1, 0x00, true, true, false, false),
        aluRow(0xFF, 0x0F, 0x2, 0x0F, false, false, false, false),
        aluRow(0xF0, 0x0F, 0x2, 0x00, true, false, false, false),
        aluRow(0xF0, 0x0F, 0x3, 0xFF, false, false, false, true),
        aluRow(0xAA, 0x55, 0x4, 0xFF, false, false, false, true),
        aluRow(0x5A, 0x5A, 0x4, 0x00, true, false, false, false),
        aluRow(0x00, 0x00, 0x5, 0xFF, false, false, false, true),
        aluRow(0x80, 0x00, 0x6, 0x00, true, true, false, false),
        aluRow(0x01, 0x00, 0x6, 0x02, false, false, false, false),
        aluRow(0xFF, 0x00, 0x6, 0xFE, false, true, false, true),
        aluRow(0x01, 0x00, 0x7, 0x00, true, true, false, false),
        aluRow(0x02, 0x00, 0x7, 0x01, false, false, false, false),
        aluRow(0xFF, 0x00, 0x7, 0x7F, false, true, false, false),
        aluRow(0x80, 0x00, 0x8, 0xC0, false, false, false, true),
        aluRow(0x7F, 0x00, 0x8, 0x3F, false, true, false, false),
        aluRow(0xFF, 0x00, 0x9, 0x00, true, true, false, false),
        aluRow(0x7F, 0x00, 0x9, 0x80, false, false, true, true),
        aluRow(0x00, 0x00, 0xA, 0xFF, false, false, false, true),
        aluRow(0x80, 0x00, 0xA, 0x7F, false, true, true, false),
        aluRow(0x01, 0x00, 0xB, 0xFF, false, true, false, true),
        aluRow(0x80, 0x00, 0xB, 0x80, false, true, true, true),
        aluRow(0xFF, 0x00, 0xB, 0x01, false, true, false, false),
        aluRow(0x42, 0xFF, 0xC, 0x42, false, false, false, false),
        aluRow(0xFF, 0x42, 0xD, 0x42, false, false, false, false),
        aluRow(0x05, 0x03, 0xE, 0x02, false, true, false, false),
        aluRow(0xFF, 0xFF, 0xF, 0x00, true, false, false, false),
    };
    const auto xor_input = bus8With(0, LogicValue::UNKNOWN);
    auto xor_output = circuit::test::logicBits(8, 0);
    xor_output[0] = LogicValue::UNKNOWN;
    rows.push_back({
        {
            {"A", pinVector(xor_input)},
            {"B", bits(0)},
            {"OP", bits(0x4)},
        },
        {
            {"OUT", pinVector(xor_output)},
            {"ZERO", pinLogic(LogicValue::UNKNOWN)},
            {"CARRY", bit(false)},
            {"OVERFLOW", bit(false)},
            {"NEGATIVE", bit(false)},
        },
    });
    const auto pass_input = bus8With(7, LogicValue::HIGH_Z);
    auto pass_output = circuit::test::logicBits(8, 0);
    pass_output[7] = LogicValue::UNKNOWN;
    rows.push_back({
        {
            {"A", pinVector(pass_input)},
            {"B", bits(0)},
            {"OP", bits(0xC)},
        },
        {
            {"OUT", pinVector(pass_output)},
            {"ZERO", pinLogic(LogicValue::UNKNOWN)},
            {"CARRY", bit(false)},
            {"OVERFLOW", bit(false)},
            {"NEGATIVE", pinLogic(LogicValue::UNKNOWN)},
        },
    });
    return rows;
}
