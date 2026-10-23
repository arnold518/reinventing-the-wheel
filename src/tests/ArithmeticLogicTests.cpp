#include "tests/ArithmeticLogicTests.hpp"

#include "modules/basic/Decoder.hpp"
#include "modules/basic/Mux.hpp"
#include "tests/TestHelpers.hpp"
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {
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

TestRow adderRow(uint64_t a, uint64_t b, bool cin, uint64_t sum, bool cout) {
    return {{{"A", pinBits(a)}, {"B", pinBits(b)}, {"C_IN", pinBit(cin)}},
            {{"OUT", pinBits(sum)}, {"C_OUT", pinBit(cout)}}};
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

}

std::vector<TestRow> halfAdderRows();
std::vector<TestRow> fullAdderRows();
std::vector<TestRow> adder8Rows();
std::vector<TestRow> zeroDetect8Rows();

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

Mux2Width1Test::Mux2Width1Test()
    : TruthTableComponentTest<Mux<2,1>>("Mux2Width1Test", "MUX_N2_W1_ROOT", oneBitMuxRows(2)) {}

Mux4Width1Test::Mux4Width1Test()
    : TruthTableComponentTest<Mux<4,1>>("Mux4Width1Test", "MUX_N4_W1_ROOT", oneBitMuxRows(4)) {}

Mux8Width1Test::Mux8Width1Test()
    : TruthTableComponentTest<Mux<8,1>>("Mux8Width1Test", "MUX_N8_W1_ROOT", oneBitMuxRows(8)) {}

Mux32Width1Test::Mux32Width1Test()
    : TruthTableComponentTest<Mux<32,1>>("Mux32Width1Test", "MUX_N32_W1_ROOT", oneBitMuxRows(32)) {}

Mux2Width4Test::Mux2Width4Test()
    : TruthTableComponentTest<Mux<2,4>>("Mux2Width4Test", "MUX_N2_W4_ROOT", busMux4Rows(2)) {}

Mux2Width32Test::Mux2Width32Test()
    : TruthTableComponentTest<Mux<2,32>>("Mux2Width32Test", "MUX_N2_W32_ROOT", busMux32Rows(2)) {}

Mux4Width32Test::Mux4Width32Test()
    : TruthTableComponentTest<Mux<4,32>>("Mux4Width32Test", "MUX_N4_W32_ROOT", busMux32Rows(4)) {}

Mux8Width8Test::Mux8Width8Test()
    : TruthTableComponentTest<Mux<8,8>>("Mux8Width8Test", "MUX_N8_W8_ROOT", busMuxRows(8)) {}

Mux8Width32Test::Mux8Width32Test()
    : TruthTableComponentTest<Mux<8,32>>("Mux8Width32Test", "MUX_N8_W32_ROOT", busMux32Rows(8)) {}

Mux32Width32Test::Mux32Width32Test()
    : TruthTableComponentTest<Mux<32,32>>("Mux32Width32Test", "MUX_N32_W32_ROOT", busMux32Rows(32)) {}

DecoderWidth5Test::DecoderWidth5Test()
    : TruthTableComponentTest<Decoder<5>>("DecoderWidth5Test", "DECODER_ADDR5_ROOT", decoderRows(32),
                                            {}, "logic.decoder.5.full") {}

namespace {
// The register file's decoder never drives output 0: x0 has no storage.
std::vector<TestRow> decoderX0UnusedRows() {
    auto rows = decoderRows(32);
    for (auto& row : rows) row.outputs["OUT0"] = bit(false);
    return rows;
}
}

DecoderWidth5ZeroDisabledTest::DecoderWidth5ZeroDisabledTest()
    : TruthTableComponentTest<Decoder<5>>("DecoderWidth5ZeroDisabledTest", "DECODER_ADDR5_ZERO_DISABLED_ROOT",
                                            decoderX0UnusedRows(), {}, "logic.decoder.5.zero-disabled") {}

RippleCarryAdderWidth8Test::RippleCarryAdderWidth8Test()
    : TruthTableComponentTest<WordTestTarget<circuit::word::Unit::RippleAdder,8>>(
          "RippleCarryAdderWidth8Test", "RIPPLE_CARRY_ADDER_W8_ROOT", adder8Rows()) {}

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
                {"C_IN", bit(false)},
            },
            {
                {"OUT", pinVector(expected)},
                {"C_OUT", bit(false)},
            },
        });
    }
    return rows;
}

ZeroDetectWidth8Test::ZeroDetectWidth8Test()
    : TruthTableComponentTest<WordTestTarget<circuit::word::Unit::Zero,8>>(
          "ZeroDetectWidth8Test", "ZERO_DETECT_W8_ROOT", zeroDetect8Rows()) {}

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
