#include "tests/RV32IALUTests.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/IOComponent.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/rv32i/RV32IALU.hpp"
#include "modules/composite/WordComponent.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "tests/SettledSweep.hpp"
#include <random>
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr uint64_t MASK32 = 0xffffffffULL;
using LogicVector = std::vector<LogicValue>;

bool known(LogicValue value) {
    return value == LogicValue::LOW || value == LogicValue::HIGH;
}

LogicValue logicNot(LogicValue value) {
    if (value == LogicValue::LOW) {
        return LogicValue::HIGH;
    }
    if (value == LogicValue::HIGH) {
        return LogicValue::LOW;
    }
    return LogicValue::UNKNOWN;
}

LogicValue logicAnd(LogicValue left, LogicValue right) {
    if (left == LogicValue::LOW || right == LogicValue::LOW) {
        return LogicValue::LOW;
    }
    if (left == LogicValue::HIGH && right == LogicValue::HIGH) {
        return LogicValue::HIGH;
    }
    return LogicValue::UNKNOWN;
}

LogicValue logicOr(LogicValue left, LogicValue right) {
    if (left == LogicValue::HIGH || right == LogicValue::HIGH) {
        return LogicValue::HIGH;
    }
    if (left == LogicValue::LOW && right == LogicValue::LOW) {
        return LogicValue::LOW;
    }
    return LogicValue::UNKNOWN;
}

LogicValue logicXor(LogicValue left, LogicValue right) {
    if (!known(left) || !known(right)) {
        return LogicValue::UNKNOWN;
    }
    return left == right ? LogicValue::LOW : LogicValue::HIGH;
}

LogicValue muxGate(
    LogicValue when_low,
    LogicValue when_high,
    LogicValue select) {
    return logicOr(
        logicAnd(when_low, logicNot(select)),
        logicAnd(when_high, select));
}

std::pair<LogicValue, LogicValue> fullAdderExpected(
    LogicValue left,
    LogicValue right,
    LogicValue carry_in) {
    const auto first_sum = logicXor(left, right);
    const auto first_carry = logicAnd(left, right);
    const auto sum = logicXor(first_sum, carry_in);
    const auto second_carry = logicAnd(first_sum, carry_in);
    return {sum, logicOr(first_carry, second_carry)};
}

struct AddExpected {
    LogicVector out;
    LogicValue carry_out = LogicValue::UNKNOWN;
    LogicValue overflow = LogicValue::UNKNOWN;
};

AddExpected addExpected(
    const LogicVector& left,
    const LogicVector& right,
    LogicValue carry_in,
    bool xor_right_with_carry) {
    assert(left.size() == right.size());
    AddExpected result;
    result.out.reserve(left.size());
    auto carry = carry_in;
    auto carry_into_sign = LogicValue::UNKNOWN;
    for (size_t bit_index = 0; bit_index < left.size(); ++bit_index) {
        if (bit_index + 1 == left.size()) {
            carry_into_sign = carry;
        }
        const auto effective_right = xor_right_with_carry
            ? logicXor(right[bit_index], carry_in)
            : right[bit_index];
        const auto [sum, next_carry] = fullAdderExpected(
            left[bit_index], effective_right, carry);
        result.out.push_back(sum);
        carry = next_carry;
    }
    result.carry_out = carry;
    result.overflow = logicXor(carry_into_sign, carry);
    return result;
}

std::array<LogicVector, 3> shiftExpected(
    const LogicVector& input,
    const LogicVector& amount) {
    assert(input.size() == 32);
    assert(amount.size() >= 5);
    LogicVector left = input;
    LogicVector right = input;
    LogicVector arithmetic = input;
    for (size_t stage = 0; stage < 5; ++stage) {
        const size_t distance = size_t{1} << stage;
        LogicVector next_left(32, LogicValue::LOW);
        LogicVector next_right(32, LogicValue::LOW);
        LogicVector next_arithmetic(32, input[31]);
        for (size_t bit = 0; bit < 32; ++bit) {
            const auto shifted_left = bit >= distance
                ? left[bit - distance]
                : LogicValue::LOW;
            const auto shifted_right = bit + distance < 32
                ? right[bit + distance]
                : LogicValue::LOW;
            const auto shifted_arithmetic = bit + distance < 32
                ? arithmetic[bit + distance]
                : input[31];
            next_left[bit] =
                muxGate(left[bit], shifted_left, amount[stage]);
            next_right[bit] =
                muxGate(right[bit], shifted_right, amount[stage]);
            next_arithmetic[bit] =
                muxGate(
                    arithmetic[bit],
                    shifted_arithmetic,
                    amount[stage]);
        }
        left = std::move(next_left);
        right = std::move(next_right);
        arithmetic = std::move(next_arithmetic);
    }
    return {left, right, arithmetic};
}

LogicVector fourStateWord(size_t offset = 0) {
    constexpr std::array<LogicValue, 4> Values{
        LogicValue::LOW,
        LogicValue::HIGH,
        LogicValue::UNKNOWN,
        LogicValue::HIGH_Z,
    };
    LogicVector result;
    result.reserve(32);
    for (size_t bit_index = 0; bit_index < 32; ++bit_index) {
        result.push_back(Values[(bit_index + offset) % Values.size()]);
    }
    return result;
}

class AddSub4Slice : public IOComponent {
public:
    explicit AddSub4Slice(std::string name)
        : IOComponent(std::move(name), [](IOComponent* self) {
              self->addPin<4>("A", PinType::INPUT);
              self->addPin<4>("B", PinType::INPUT);
              self->addPin("SUB", PinType::INPUT);
              self->addPin<4>("OUT", PinType::OUTPUT);
              self->addPin("CARRY_OUT", PinType::OUTPUT);
              self->addPin("OVERFLOW", PinType::OUTPUT);
          }) {}

    static constexpr const char* TypeName = "AddSub4Slice";
    const char* getTypeName() const override { return TypeName; }

    void buildInternals(ComponentBuilder& builder) override {
        builder.addNewComponent<BitSplitter<4>>("A_SPLIT");
        builder.addNewComponent<BitSplitter<4>>("B_SPLIT");
        builder.addNewComponent<BitJoiner<4>>("OUT_JOIN");
        builder.addNewComponent<XORGate>("OVERFLOW_XOR");

        builder.addNewWire<4>(
            "A_bus_internal",
            getInputPin<4>("A"),
            {builder.getInputPin<BitSplitter<4>, 4>("A_SPLIT", "IN")});
        builder.addNewWire<4>(
            "B_bus_internal",
            getInputPin<4>("B"),
            {builder.getInputPin<BitSplitter<4>, 4>("B_SPLIT", "IN")});

        std::vector<std::shared_ptr<Pin<>>> sub_sinks;
        sub_sinks.reserve(5);
        for (size_t i = 0; i < 4; ++i) {
            const auto bit = std::to_string(i);
            const auto xor_name = "B_XOR_SUB_" + bit;
            const auto adder_name = "FA" + bit;

            builder.addNewComponent<XORGate>(xor_name);
            builder.addNewComponent<FullAdder>(adder_name);
            sub_sinks.push_back(builder.getInputPin<XORGate>(xor_name, "B"));

            builder.addNewWire(
                "A_bit_" + bit,
                builder.getOutputPin<BitSplitter<4>>("A_SPLIT", "OUT_" + bit),
                {builder.getInputPin<FullAdder>(adder_name, "A")});
            builder.addNewWire(
                "B_bit_" + bit,
                builder.getOutputPin<BitSplitter<4>>("B_SPLIT", "OUT_" + bit),
                {builder.getInputPin<XORGate>(xor_name, "A")});
            builder.addNewWire(
                "B_xor_SUB_" + bit + "_to_FA",
                builder.getOutputPin<XORGate>(xor_name, "OUT"),
                {builder.getInputPin<FullAdder>(adder_name, "B")});
            builder.addNewWire(
                "SUM_bit_" + bit,
                builder.getOutputPin<FullAdder>(adder_name, "Sum"),
                {builder.getInputPin<BitJoiner<4>>("OUT_JOIN", "IN_" + bit)});
        }

        sub_sinks.push_back(builder.getInputPin<FullAdder>("FA0", "Carry_in"));
        builder.addNewWire("SUB_control", getInputPin("SUB"), sub_sinks);

        for (size_t i = 0; i < 3; ++i) {
            const auto from = std::to_string(i);
            const auto to = std::to_string(i + 1);
            std::vector<std::shared_ptr<Pin<>>> sinks{
                builder.getInputPin<FullAdder>("FA" + to, "Carry_in")};
            if (i == 2) {
                sinks.push_back(builder.getInputPin<XORGate>("OVERFLOW_XOR", "A"));
            }
            builder.addNewWire(
                "carry_" + from + "_to_" + to,
                builder.getOutputPin<FullAdder>("FA" + from, "Carry_out"),
                sinks);
        }

        builder.addNewWire(
            "carry_out_fanout",
            builder.getOutputPin<FullAdder>("FA3", "Carry_out"),
            {getOutputPin("CARRY_OUT"),
             builder.getInputPin<XORGate>("OVERFLOW_XOR", "B")});
        builder.addNewWire(
            "overflow_to_output",
            builder.getOutputPin<XORGate>("OVERFLOW_XOR", "OUT"),
            {getOutputPin("OVERFLOW")});
        builder.addNewWire<4>(
            "out_bus_internal",
            builder.getOutputPin<BitJoiner<4>, 4>("OUT_JOIN", "OUT"),
            {getOutputPin<4>("OUT")});
    }
};

void expect(bool condition, const char* test_name) {
    if (!condition) {
        throw std::runtime_error(std::string(test_name) + " failed");
    }
}

bool addCarry(uint32_t a, uint32_t b) {
    return (static_cast<uint64_t>(a) + static_cast<uint64_t>(b)) > MASK32;
}

bool addOverflow(uint32_t a, uint32_t b, uint32_t result) {
    return (~(a ^ b) & (a ^ result) & 0x80000000U) != 0;
}

bool subCarry(uint32_t a, uint32_t b) {
    return a >= b;
}

bool subOverflow(uint32_t a, uint32_t b, uint32_t result) {
    return ((a ^ b) & (a ^ result) & 0x80000000U) != 0;
}

bool addCarry4(uint32_t a, uint32_t b) {
    return (a + b) > 0xFU;
}

bool addOverflow4(uint32_t a, uint32_t b, uint32_t result) {
    return (~(a ^ b) & (a ^ result) & 0x8U) != 0;
}

bool subCarry4(uint32_t a, uint32_t b) {
    return a >= b;
}

bool subOverflow4(uint32_t a, uint32_t b, uint32_t result) {
    return ((a ^ b) & (a ^ result) & 0x8U) != 0;
}

uint32_t sra(uint32_t value, uint32_t amount) {
    amount &= 0x1fU;
    return static_cast<uint32_t>(static_cast<int32_t>(value) >> amount);
}

TestRow addSubRow(uint32_t a, uint32_t b, bool sub) {
    const uint32_t result = sub ? (a - b) : (a + b);
    return {{{"A", bits(a)}, {"B", bits(b)}, {"SUB", bit(sub)}},
            {{"OUT", bits(result)},
             {"C_OUT", bit(sub ? subCarry(a, b) : addCarry(a, b))},
             {"OVERFLOW", bit(sub ? subOverflow(a, b, result) : addOverflow(a, b, result))}}};
}

TestRow adder32Row(uint32_t a, uint32_t b, bool cin) {
    const uint64_t wide = static_cast<uint64_t>(a) + static_cast<uint64_t>(b) + (cin ? 1ULL : 0ULL);
    return {{{"A", bits(a)}, {"B", bits(b)}, {"C_IN", bit(cin)}},
            {{"OUT", bits(static_cast<uint32_t>(wide))}, {"C_OUT", bit(wide > MASK32)}}};
}

TestRow addSub4Row(uint32_t a, uint32_t b, bool sub) {
    const uint32_t result = (sub ? (a - b) : (a + b)) & 0xFU;
    return {{{"A", bits(a)}, {"B", bits(b)}, {"SUB", bit(sub)}},
            {{"OUT", bits(result)},
             {"CARRY_OUT", bit(sub ? subCarry4(a, b) : addCarry4(a, b))},
             {"OVERFLOW", bit(sub ? subOverflow4(a, b, result) : addOverflow4(a, b, result))}}};
}

TestRow shifterRow(
    uint32_t a,
    uint32_t amount,
    bool left,
    bool arithmetic) {
    const uint32_t shamt = amount & 0x1fU;
    const uint32_t out = left
        ? a << shamt
        : arithmetic ? sra(a, amount) : a >> shamt;
    return {
        {{"A", bits(a)},
         {"B", bits(amount)},
         {"LEFT", bit(left)},
         {"ARITHMETIC", bit(arithmetic)}},
        {{"OUT", bits(out)}}};
}

TestRow aluRow(uint32_t a, uint32_t b, uint8_t op, uint32_t out, bool carry = false, bool overflow = false) {
    const bool comparison_valid =
        op == ::rv32i::ALUOp::SUB || op == ::rv32i::ALUOp::SLT
        || op == ::rv32i::ALUOp::SLTU;
    return {{{"A", bits(a)}, {"B", bits(b)}, {"OP", bits(op)}},
            {{"OUT", bits(out)},
             {"ZERO", bit(out == 0)},
             {"EQ", bit(comparison_valid && a == b)},
             {"LT_SIGNED", bit(comparison_valid && static_cast<int32_t>(a) < static_cast<int32_t>(b))},
             {"LT_UNSIGNED", bit(comparison_valid && a < b)},
             {"NEGATIVE", bit((out & 0x80000000U) != 0)},
             {"CARRY_OUT", bit(carry)},
             {"OVERFLOW", bit(overflow)}}};
}

uint32_t aluExpectedOut(uint32_t a, uint32_t b, uint8_t op) {
    switch (op) {
        case ::rv32i::ALUOp::ADD: return a + b;
        case ::rv32i::ALUOp::SUB: return a - b;
        case ::rv32i::ALUOp::AND: return a & b;
        case ::rv32i::ALUOp::OR: return a | b;
        case ::rv32i::ALUOp::XOR: return a ^ b;
        case ::rv32i::ALUOp::SLL: return a << (b & 0x1fU);
        case ::rv32i::ALUOp::SRL: return a >> (b & 0x1fU);
        case ::rv32i::ALUOp::SRA: return sra(a, b);
        case ::rv32i::ALUOp::SLT: return static_cast<int32_t>(a) < static_cast<int32_t>(b) ? 1U : 0U;
        case ::rv32i::ALUOp::SLTU: return a < b ? 1U : 0U;
        case ::rv32i::ALUOp::PASS_A: return a;
        case ::rv32i::ALUOp::PASS_B: return b;
        case ::rv32i::ALUOp::CLEAR: return 0;
        default: return 0;
    }
}

TestRow aluAutoRow(uint32_t a, uint32_t b, uint8_t op) {
    const uint32_t out = aluExpectedOut(a, b, op);
    bool carry = false;
    bool overflow = false;
    if (op == ::rv32i::ALUOp::ADD) {
        carry = addCarry(a, b);
        overflow = addOverflow(a, b, out);
    } else if (op == ::rv32i::ALUOp::SUB || op == ::rv32i::ALUOp::SLT || op == ::rv32i::ALUOp::SLTU) {
        const uint32_t diff = a - b;
        carry = subCarry(a, b);
        overflow = subOverflow(a, b, diff);
    }
    return aluRow(a, b, op, out, carry, overflow);
}

std::vector<TestRow> adder32Rows() {
    std::vector<TestRow> rows{
        adder32Row(0x00000000U, 0x00000000U, false),
        adder32Row(0x00000000U, 0x00000000U, true),
        adder32Row(0x00000001U, 0x00000002U, false),
        adder32Row(0xffffffffU, 0x00000001U, false),
        adder32Row(0xffffffffU, 0x00000000U, true),
        adder32Row(0xffffffffU, 0xffffffffU, false),
        adder32Row(0xffffffffU, 0xffffffffU, true),
        adder32Row(0x55555555U, 0xaaaaaaaaU, false),
        adder32Row(0x55555555U, 0xaaaaaaaaU, true),
        adder32Row(0x7fffffffU, 0x00000001U, false),
        adder32Row(0x80000000U, 0x80000000U, false),
    };

    for (uint32_t width = 1; width < 32; ++width) {
        const uint32_t carry_chain = static_cast<uint32_t>((1ULL << width) - 1ULL);
        rows.push_back(adder32Row(carry_chain, 0x00000001U, false));
    }
    for (const auto unknown_value :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        auto left = circuit::test::logicBits(32, 0);
        left[5] = unknown_value;
        const auto right = circuit::test::logicBits(32, 0);
        const auto expected =
            addExpected(left, right, LogicValue::LOW, false);
        rows.push_back({
            {
                {"A", TestValue(left)},
                {"B", TestValue(right)},
                {"C_IN", bit(false)},
            },
            {
                {"OUT", TestValue(expected.out)},
                {"C_OUT", TestValue(expected.carry_out)},
            },
        });
    }
    return rows;
}

std::vector<TestRow> addSub4Rows() {
    std::vector<TestRow> rows;
    rows.reserve(16 * 16 * 2);
    for (uint32_t a = 0; a < 16; ++a) {
        for (uint32_t b = 0; b < 16; ++b) {
            rows.push_back(addSub4Row(a, b, false));
            rows.push_back(addSub4Row(a, b, true));
        }
    }
    return rows;
}

std::vector<TestRow> addSub32Rows() {
    std::vector<TestRow> rows{
        addSubRow(0x00000000U, 0x00000000U, false),
        addSubRow(0x00000001U, 0x00000002U, false),
        addSubRow(0xffffffffU, 0x00000001U, false),
        addSubRow(0xffffffffU, 0xffffffffU, false),
        addSubRow(0x55555555U, 0xaaaaaaaaU, false),
        addSubRow(0x55555555U, 0xaaaaaaaaU, true),
        addSubRow(0x7fffffffU, 0x00000001U, false),
        addSubRow(0x80000000U, 0x80000000U, false),
        addSubRow(0x7fffffffU, 0xffffffffU, false),
        addSubRow(0x00000005U, 0x00000003U, true),
        addSubRow(0x00000000U, 0x00000001U, true),
        addSubRow(0x80000000U, 0x00000001U, true),
        addSubRow(0x7fffffffU, 0xffffffffU, true),
        addSubRow(0x80000000U, 0x7fffffffU, true),
        addSubRow(0xffffffffU, 0xffffffffU, true),
    };

    for (uint32_t width = 1; width < 32; ++width) {
        const uint32_t carry_chain = static_cast<uint32_t>((1ULL << width) - 1ULL);
        const uint32_t borrow_chain = static_cast<uint32_t>(1ULL << width);
        rows.push_back(addSubRow(carry_chain, 0x00000001U, false));
        rows.push_back(addSubRow(borrow_chain, 0x00000001U, true));
    }
    for (const auto unknown_value :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        auto left = circuit::test::logicBits(32, 0);
        left[5] = unknown_value;
        const auto right = circuit::test::logicBits(32, 0);
        const auto expected =
            addExpected(left, right, LogicValue::LOW, true);
        rows.push_back({
            {
                {"A", TestValue(left)},
                {"B", TestValue(right)},
                {"SUB", bit(false)},
            },
            {
                {"OUT", TestValue(expected.out)},
                {"C_OUT", TestValue(expected.carry_out)},
                {"OVERFLOW", TestValue(expected.overflow)},
            },
        });
    }
    const auto zero = circuit::test::logicBits(32, 0);
    const auto unknown_sub_expected =
        addExpected(zero, zero, LogicValue::UNKNOWN, true);
    rows.push_back({
        {
            {"A", TestValue(zero)},
            {"B", TestValue(zero)},
            {"SUB", TestValue(LogicValue::UNKNOWN)},
        },
        {
            {"OUT", TestValue(unknown_sub_expected.out)},
            {"C_OUT", TestValue(
                unknown_sub_expected.carry_out)},
            {"OVERFLOW", TestValue(
                unknown_sub_expected.overflow)},
        },
    });
    return rows;
}

std::vector<TestRow> logic32Rows() {
    std::vector<TestRow> rows{
        {{{"A", bits(0xf0f0f0f0U)}, {"B", bits(0x0ff00ff0U)}},
         {{"AND_OUT", bits(0x00f000f0U)}, {"OR_OUT", bits(0xfff0fff0U)}, {"XOR_OUT", bits(0xff00ff00U)}}},
        {{{"A", bits(0xffffffffU)}, {"B", bits(0x00000000U)}},
         {{"AND_OUT", bits(0x00000000U)}, {"OR_OUT", bits(0xffffffffU)}, {"XOR_OUT", bits(0xffffffffU)}}},
        {{{"A", bits(0xaaaaaaaaU)}, {"B", bits(0x55555555U)}},
         {{"AND_OUT", bits(0x00000000U)}, {"OR_OUT", bits(0xffffffffU)}, {"XOR_OUT", bits(0xffffffffU)}}},
        {{{"A", bits(0x80000000U)}, {"B", bits(0x7fffffffU)}},
         {{"AND_OUT", bits(0x00000000U)}, {"OR_OUT", bits(0xffffffffU)}, {"XOR_OUT", bits(0xffffffffU)}}},
    };

    for (uint32_t bit_index = 0; bit_index < 32; ++bit_index) {
        const uint32_t mask = uint32_t{1} << bit_index;
        rows.push_back({{{"A", bits(mask)}, {"B", bits(0x00000000U)}},
                        {{"AND_OUT", bits(0x00000000U)}, {"OR_OUT", bits(mask)}, {"XOR_OUT", bits(mask)}}});
        rows.push_back({{{"A", bits(mask)}, {"B", bits(mask)}},
                        {{"AND_OUT", bits(mask)}, {"OR_OUT", bits(mask)}, {"XOR_OUT", bits(0x00000000U)}}});
    }
    const auto left = fourStateWord(0);
    const auto right = fourStateWord(2);
    LogicVector and_result;
    LogicVector or_result;
    LogicVector xor_result;
    for (size_t bit_index = 0; bit_index < 32; ++bit_index) {
        and_result.push_back(logicAnd(left[bit_index], right[bit_index]));
        or_result.push_back(logicOr(left[bit_index], right[bit_index]));
        xor_result.push_back(logicXor(left[bit_index], right[bit_index]));
    }
    rows.push_back({
        {{"A", TestValue(left)}, {"B", TestValue(right)}},
        {
            {"AND_OUT", TestValue(and_result)},
            {"OR_OUT", TestValue(or_result)},
            {"XOR_OUT", TestValue(xor_result)},
        },
    });
    return rows;
}

std::vector<TestRow> zeroDetect32Rows() {
    std::vector<TestRow> rows{
        {{{"A", bits(0x00000000U)}}, {{"ZERO", bit(true)}}},
        {{{"A", bits(0x00000001U)}}, {{"ZERO", bit(false)}}},
        {{{"A", bits(0x80000000U)}}, {{"ZERO", bit(false)}}},
        {{{"A", bits(0xffffffffU)}}, {{"ZERO", bit(false)}}},
    };

    for (uint32_t bit_index = 0; bit_index < 32; ++bit_index) {
        rows.push_back({{{"A", bits(uint32_t{1} << bit_index)}}, {{"ZERO", bit(false)}}});
    }
    for (const auto unknown_value :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        auto only_unknown = circuit::test::logicBits(32, 0);
        only_unknown[13] = unknown_value;
        rows.push_back({
            {{"A", TestValue(only_unknown)}},
            {{"ZERO", TestValue(LogicValue::UNKNOWN)}},
        });

        auto high_dominates = only_unknown;
        high_dominates[31] = LogicValue::HIGH;
        rows.push_back({
            {{"A", TestValue(high_dominates)}},
            {{"ZERO", bit(false)}},
        });
    }
    return rows;
}

std::vector<TestRow> shifter32Rows() {
    std::vector<TestRow> rows;
    rows.reserve((32 + 4) * 12);
    for (uint32_t amount = 0; amount < 32; ++amount) {
        for (uint32_t value : {
                 0x80000001U, 0x7fffffffU,
                 0xffffffffU, 0x00000001U}) {
            rows.push_back(shifterRow(value, amount, true, false));
            rows.push_back(shifterRow(value, amount, false, false));
            rows.push_back(shifterRow(value, amount, false, true));
        }
    }
    for (uint32_t amount : {32U, 33U, 63U, 0xffffffffU}) {
        for (uint32_t value : {
                 0x80000001U, 0x7fffffffU,
                 0xffffffffU, 0x00000001U}) {
            rows.push_back(shifterRow(value, amount, true, false));
            rows.push_back(shifterRow(value, amount, false, false));
            rows.push_back(shifterRow(value, amount, false, true));
        }
    }
    for (const auto amount_value :
         {uint32_t{0}, uint32_t{5}, uint32_t{31}}) {
        const auto input = fourStateWord(amount_value % 4);
        const auto amount =
            circuit::test::logicBits(32, amount_value);
        const auto expected = shiftExpected(input, amount);
        for (size_t mode = 0; mode < 3; ++mode) {
            rows.push_back({
                {
                    {"A", TestValue(input)},
                    {"B", TestValue(amount)},
                    {"LEFT", TestValue(bit(mode == 0))},
                    {"ARITHMETIC", TestValue(bit(mode == 2))},
                },
                {{"OUT", TestValue(expected[mode])}},
            });
        }
    }
    for (const auto unknown_value :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        const auto input = circuit::test::logicBits(32, 0);
        auto amount = circuit::test::logicBits(32, 0);
        amount[2] = unknown_value;
        const auto expected = shiftExpected(input, amount);
        for (size_t mode = 0; mode < 3; ++mode) {
            rows.push_back({
                {
                    {"A", TestValue(input)},
                    {"B", TestValue(amount)},
                    {"LEFT", TestValue(bit(mode == 0))},
                    {"ARITHMETIC", TestValue(bit(mode == 2))},
                },
                {{"OUT", TestValue(expected[mode])}},
            });
        }
    }
    return rows;
}

std::vector<TestRow> integerALURows() {
    std::vector<TestRow> rows{
        aluAutoRow(0x00000001U, 0x00000002U, ::rv32i::ALUOp::ADD),
        aluAutoRow(0xffffffffU, 0x00000001U, ::rv32i::ALUOp::ADD),
        aluAutoRow(0xffffffffU, 0xffffffffU, ::rv32i::ALUOp::ADD),
        aluAutoRow(0x7fffffffU, 0x00000001U, ::rv32i::ALUOp::ADD),
        aluAutoRow(0x80000000U, 0x80000000U, ::rv32i::ALUOp::ADD),
        aluAutoRow(0x00000005U, 0x00000005U, ::rv32i::ALUOp::SUB),
        aluAutoRow(0x00000000U, 0x00000001U, ::rv32i::ALUOp::SUB),
        aluAutoRow(0x80000000U, 0x7fffffffU, ::rv32i::ALUOp::SUB),
        aluAutoRow(0x7fffffffU, 0xffffffffU, ::rv32i::ALUOp::SUB),
        aluAutoRow(0xf0f0f0f0U, 0x0ff00ff0U, ::rv32i::ALUOp::AND),
        aluAutoRow(0xf0f0f0f0U, 0x0ff00ff0U, ::rv32i::ALUOp::OR),
        aluAutoRow(0xf0f0f0f0U, 0x0ff00ff0U, ::rv32i::ALUOp::XOR),
        aluAutoRow(0x00000001U, 0x0000001fU, ::rv32i::ALUOp::SLL),
        aluAutoRow(0x00000001U, 0x00000020U, ::rv32i::ALUOp::SLL),
        aluAutoRow(0x80000000U, 0x0000001fU, ::rv32i::ALUOp::SRL),
        aluAutoRow(0x80000000U, 0x00000021U, ::rv32i::ALUOp::SRL),
        aluAutoRow(0x80000000U, 0x0000001fU, ::rv32i::ALUOp::SRA),
        aluAutoRow(0x80000000U, 0x00000021U, ::rv32i::ALUOp::SRA),
        aluAutoRow(0xffffffffU, 0x00000001U, ::rv32i::ALUOp::SLT),
        aluAutoRow(0x80000000U, 0x00000000U, ::rv32i::ALUOp::SLT),
        aluAutoRow(0x7fffffffU, 0x80000000U, ::rv32i::ALUOp::SLT),
        aluAutoRow(0xffffffffU, 0x00000001U, ::rv32i::ALUOp::SLTU),
        aluAutoRow(0x00000000U, 0xffffffffU, ::rv32i::ALUOp::SLTU),
        aluAutoRow(0x7fffffffU, 0x80000000U, ::rv32i::ALUOp::SLTU),
        aluAutoRow(0x12345678U, 0x9abcdef0U, ::rv32i::ALUOp::PASS_A),
        aluAutoRow(0x12345678U, 0x9abcdef0U, ::rv32i::ALUOp::PASS_B),
        aluAutoRow(0x12345678U, 0x9abcdef0U, ::rv32i::ALUOp::CLEAR),
    };

    for (uint8_t op = 0x0d; op <= 0x1f; ++op) {
        rows.push_back(aluAutoRow(0x12345678U, 0x9abcdef0U, op));
    }
    return rows;
}

std::vector<TestRow> control32Rows() {
    std::vector<TestRow> rows;
    for (unsigned op = 0; op < 32; ++op) {
        const bool arithmetic = op == 0 || op == 1 || op == 8 || op == 9;
        rows.push_back({{{"OP", bits(op)}}, {
            {"LOCAL_OP", bits(op & 7)}, {"HIGH_HALF", bit(op & 8)}, {"OUT_OF_RANGE", bit(op & 16)},
            {"SUB", bit(arithmetic && op != 0)},
            {"SHIFT_LEFT", bit(!(op & 2))}, {"SHIFT_ARITHMETIC", bit(op & 1)}}});
    }
    return rows;
}
std::vector<TestRow> compare32Rows() {
    std::vector<TestRow> rows;
    for (unsigned controls = 0; controls < 8; ++controls)
        for (unsigned diff : {0u, 1u, 0x80000000u, 0xffffffffu}) {
            const bool enable = controls & 1, carry = controls & 2, overflow = controls & 4;
            const bool lt = enable && (bool(diff & 0x80000000u) != overflow);
            const bool ltu = enable && !carry;
            rows.push_back({{{"DIFF", bits(diff)}, {"CARRY_IN", bit(carry)},
                {"OVERFLOW_IN", bit(overflow)}, {"ENABLE", bit(enable)}}, {
                {"EQ", bit(enable && diff == 0)}, {"LT_SIGNED", bit(lt)}, {"LT_UNSIGNED", bit(ltu)},
                {"SLT_RESULT", bits(lt)}, {"SLTU_RESULT", bits(ltu)}}});
        }
    for (auto unknown : {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        auto row = rows.front();
        row.inputs["DIFF"] = TestValue(std::vector<LogicValue>(32, unknown));
        row.inputs["CARRY_IN"] = unknown;
        row.inputs["OVERFLOW_IN"] = unknown;
        rows.push_back(row);
    }
    return rows;
}
std::vector<TestRow> selector32Rows() {
    constexpr std::array<const char*, 9> names{"ARITHMETIC_RESULT", "AND_RESULT", "OR_RESULT", "XOR_RESULT",
        "SHIFT_RESULT", "SLT_RESULT", "SLTU_RESULT", "A", "B"};
    constexpr std::array<int, 16> selected{0, 0, 1, 2, 3, 4, 4, 4, 5, 6, 7, 8, -1, -1, -1, -1};
    std::vector<TestRow> rows;
    for (unsigned pattern : {0x12345678u, 0xedcba987u}) for (unsigned op = 0; op < 32; ++op) {
        TestRow row;
        for (unsigned i = 0; i < names.size(); ++i) row.inputs[names[i]] = bits(pattern ^ (0x11111111u * i));
        row.inputs["LOCAL_OP"] = bits(op & 7);
        row.inputs["HIGH_HALF"] = bit(op & 8);
        row.inputs["OUT_OF_RANGE"] = bit(op & 16);
        row.outputs["OUT"] = op < 16 && selected[op] >= 0
            ? row.inputs.at(names[selected[op]]) : bits(0);
        rows.push_back(row);
    }
    for (auto unknown : {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        auto row = rows[16];
        for (auto name : names) row.inputs[name] = TestValue(std::vector<LogicValue>(32, unknown));
        rows.push_back(row);
    }
    return rows;
}
circuit::test::ComponentTestSpec rv32iALUTestSpec() {
    auto rows = integerALURows();
    for (auto& row : rows)
        for (const auto* flag : {"ZERO", "NEGATIVE", "CARRY_OUT", "OVERFLOW"})
            row.outputs.erase(flag);
    circuit::test::ComponentTestSpec spec{
        "RV32IALUTest", std::string(circuit::families::rv32iALU().id()),
        "RV32I_ALU_ROOT", {}, {}};
    spec.scenarios.push_back(circuit::test::actionScenarioFromRows(
        "truth-table", spec.contract_id, rows, {100'000, 2'000'000}));
    return spec;
}
}

ParallelPrefixAdderWidth32Test::ParallelPrefixAdderWidth32Test()
    : TruthTableComponentTest("ParallelPrefixAdderWidth32Test", "PARALLEL_PREFIX_ADDER_W32_ROOT", adder32Rows()) {}
ZeroDetectWidth32Test::ZeroDetectWidth32Test()
    : TruthTableComponentTest("ZeroDetectWidth32Test", "ZERO_DETECT_W32_ROOT", zeroDetect32Rows()) {}
RV32IALUAddSubTest::RV32IALUAddSubTest()
    : TruthTableComponentTest("RV32IALUAddSubTest", "RV32I_ALU_ADD_SUB_ROOT", addSub32Rows()) {}
RV32IALULogicTest::RV32IALULogicTest()
    : TruthTableComponentTest("RV32IALULogicTest", "RV32I_ALU_LOGIC_ROOT", logic32Rows()) {}
RV32IALUShifterTest::RV32IALUShifterTest()
    : TruthTableComponentTest("RV32IALUShifterTest", "RV32I_ALU_SHIFTER_ROOT", shifter32Rows()) {}
RV32IALUControlTest::RV32IALUControlTest()
    : TruthTableComponentTest("RV32IALUControlTest", "RV32I_ALU_CONTROL_ROOT", control32Rows()) {}
RV32IALUCompareTest::RV32IALUCompareTest()
    : TruthTableComponentTest("RV32IALUCompareTest", "RV32I_ALU_COMPARE_ROOT", compare32Rows()) {}
RV32IALUResultSelectTest::RV32IALUResultSelectTest()
    : TruthTableComponentTest("RV32IALUResultSelectTest", "RV32I_ALU_RESULT_SELECT_ROOT", selector32Rows()) {}
RV32IALUTest::RV32IALUTest()
    : ComponentScenarioTest(rv32iALUTestSpec(), "truth-table") {}

std::string RV32IALUSliceTest::getTestName() const {
    return "RV32IALUSliceTest";
}

void RV32IALUSliceTest::verifyResults() {
    expect(
        runRowsBatched<AddSub4Slice>(addSub4Rows(), 100),
        "RV32IALUSliceTest AddSub4Slice exhaustive");
}

namespace {
circuit::test::NamedValues namedValues(const std::map<std::string, TestValue>& values,
                                       const circuit::test::SettledSweep& sweep) {
    circuit::test::NamedValues result;
    for (const auto& [name, value] : values) {
        if (value.hasLogicVector()) result[name] = value.values;
        else if (value.multi) result[name] = circuit::test::logicBits(sweep.width(name), value.numeric);
        else result[name] = {value.logic};
    }
    return result;
}
} // namespace

void RV32IALUSweepTest::verifyResults() {
    using namespace circuit::test;
    FidelitySweep sweep(circuit::families::rv32iALU());
    size_t rows = 0;
    auto check = [&](uint32_t a, uint32_t b, uint8_t op) {
        const auto row = aluAutoRow(a, b, op);
        const auto inputs = namedValues(row.inputs, sweep.gates());
        auto expected = namedValues(row.outputs, sweep.gates());
        // RV32IALU exposes OUT and the three comparison outputs only.
        for (const auto* flag : {"ZERO", "NEGATIVE", "CARRY_OUT", "OVERFLOW"}) expected.erase(flag);
        sweep.evaluate(inputs, &expected);
        ++rows;
    };
    const std::array<uint32_t, 6> edges{
        0x00000000U, 0x00000001U, 0x0000001fU, 0x7fffffffU, 0x80000000U, 0xffffffffU};
    for (uint8_t op = 0; op <= ::rv32i::ALUOp::CLEAR; ++op)
        for (auto a : edges)
            for (auto b : edges) check(a, b, op);
    std::mt19937 rng(0xA1B32);
    for (size_t i = 0; i < 256; ++i) {
        const uint32_t a = rng();
        const uint32_t b = rng();
        const auto op = static_cast<uint8_t>(rng() % 32);
        check(a, b, op);
    }
    std::cout << "RV32IALU: " << rows << " vectors against C++ integer results; "
              << (sweep.hasBehavioral() ? "gate-level and behavioral forms agree" : "gate-level form only") << '\n';
}
