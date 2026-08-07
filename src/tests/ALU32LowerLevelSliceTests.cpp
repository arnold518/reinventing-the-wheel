#include "tests/ALU32LowerLevelSliceTests.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/IOComponent.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/Comparator32.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/utility/BitAdapter.hpp"
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

LogicValue zeroExpected(const LogicVector& values) {
    bool has_unknown = false;
    for (const auto value : values) {
        if (value == LogicValue::HIGH) {
            return LogicValue::LOW;
        }
        if (value != LogicValue::LOW) {
            has_unknown = true;
        }
    }
    return has_unknown ? LogicValue::UNKNOWN : LogicValue::HIGH;
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
             {"CARRY_OUT", bit(sub ? subCarry(a, b) : addCarry(a, b))},
             {"OVERFLOW", bit(sub ? subOverflow(a, b, result) : addOverflow(a, b, result))}}};
}

TestRow adder32Row(uint32_t a, uint32_t b, bool cin) {
    const uint64_t wide = static_cast<uint64_t>(a) + static_cast<uint64_t>(b) + (cin ? 1ULL : 0ULL);
    return {{{"A", bits(a)}, {"B", bits(b)}, {"Cin", bit(cin)}},
            {{"Sum", bits(static_cast<uint32_t>(wide))}, {"Cout", bit(wide > MASK32)}}};
}

TestRow addSub4Row(uint32_t a, uint32_t b, bool sub) {
    const uint32_t result = (sub ? (a - b) : (a + b)) & 0xFU;
    return {{{"A", bits(a)}, {"B", bits(b)}, {"SUB", bit(sub)}},
            {{"OUT", bits(result)},
             {"CARRY_OUT", bit(sub ? subCarry4(a, b) : addCarry4(a, b))},
             {"OVERFLOW", bit(sub ? subOverflow4(a, b, result) : addOverflow4(a, b, result))}}};
}

TestRow comparatorRow(uint32_t a, uint32_t b) {
    const uint32_t diff = a - b;
    return {{{"A", bits(a)}, {"B", bits(b)}},
            {{"EQ", bit(a == b)},
             {"LT_SIGNED", bit(static_cast<int32_t>(a) < static_cast<int32_t>(b))},
             {"LT_UNSIGNED", bit(a < b)},
             {"DIFF", bits(diff)},
             {"CARRY_OUT", bit(subCarry(a, b))},
             {"OVERFLOW", bit(subOverflow(a, b, diff))}}};
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
        op == ALU32Op::SUB || op == ALU32Op::SLT
        || op == ALU32Op::SLTU;
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
        case ALU32Op::ADD: return a + b;
        case ALU32Op::SUB: return a - b;
        case ALU32Op::AND: return a & b;
        case ALU32Op::OR: return a | b;
        case ALU32Op::XOR: return a ^ b;
        case ALU32Op::SLL: return a << (b & 0x1fU);
        case ALU32Op::SRL: return a >> (b & 0x1fU);
        case ALU32Op::SRA: return sra(a, b);
        case ALU32Op::SLT: return static_cast<int32_t>(a) < static_cast<int32_t>(b) ? 1U : 0U;
        case ALU32Op::SLTU: return a < b ? 1U : 0U;
        case ALU32Op::PASS_A: return a;
        case ALU32Op::PASS_B: return b;
        case ALU32Op::ZERO: return 0;
        default: return 0;
    }
}

TestRow aluAutoRow(uint32_t a, uint32_t b, uint8_t op) {
    const uint32_t out = aluExpectedOut(a, b, op);
    bool carry = false;
    bool overflow = false;
    if (op == ALU32Op::ADD) {
        carry = addCarry(a, b);
        overflow = addOverflow(a, b, out);
    } else if (op == ALU32Op::SUB || op == ALU32Op::SLT || op == ALU32Op::SLTU) {
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
                {"Cin", bit(false)},
            },
            {
                {"Sum", TestValue(expected.out)},
                {"Cout", TestValue(expected.carry_out)},
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
                {"CARRY_OUT", TestValue(expected.carry_out)},
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
            {"CARRY_OUT", TestValue(
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

std::vector<TestRow> comparator32Rows() {
    std::vector<TestRow> rows;
    constexpr uint32_t values[] = {
        0x00000000U,
        0x00000001U,
        0x00000002U,
        0x7fffffffU,
        0x80000000U,
        0xffffffffU,
    };
    for (uint32_t a : values) {
        for (uint32_t b : values) {
            rows.push_back(comparatorRow(a, b));
        }
    }
    for (const auto unknown_value :
         {LogicValue::UNKNOWN, LogicValue::HIGH_Z}) {
        auto left = circuit::test::logicBits(32, 0x80000000U);
        left[5] = unknown_value;
        const auto right = circuit::test::logicBits(32, 0);
        const auto subtraction =
            addExpected(left, right, LogicValue::HIGH, true);
        rows.push_back({
            {{"A", TestValue(left)}, {"B", TestValue(right)}},
            {
                {"EQ", TestValue(zeroExpected(subtraction.out))},
                {"LT_SIGNED", TestValue(logicXor(
                    subtraction.out[31], subtraction.overflow))},
                {"LT_UNSIGNED", TestValue(logicNot(
                    subtraction.carry_out))},
                {"DIFF", TestValue(subtraction.out)},
                {"CARRY_OUT", TestValue(
                    subtraction.carry_out)},
                {"OVERFLOW", TestValue(subtraction.overflow)},
            },
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

std::vector<TestRow> alu32Rows() {
    std::vector<TestRow> rows{
        aluAutoRow(0x00000001U, 0x00000002U, ALU32Op::ADD),
        aluAutoRow(0xffffffffU, 0x00000001U, ALU32Op::ADD),
        aluAutoRow(0xffffffffU, 0xffffffffU, ALU32Op::ADD),
        aluAutoRow(0x7fffffffU, 0x00000001U, ALU32Op::ADD),
        aluAutoRow(0x80000000U, 0x80000000U, ALU32Op::ADD),
        aluAutoRow(0x00000005U, 0x00000005U, ALU32Op::SUB),
        aluAutoRow(0x00000000U, 0x00000001U, ALU32Op::SUB),
        aluAutoRow(0x80000000U, 0x7fffffffU, ALU32Op::SUB),
        aluAutoRow(0x7fffffffU, 0xffffffffU, ALU32Op::SUB),
        aluAutoRow(0xf0f0f0f0U, 0x0ff00ff0U, ALU32Op::AND),
        aluAutoRow(0xf0f0f0f0U, 0x0ff00ff0U, ALU32Op::OR),
        aluAutoRow(0xf0f0f0f0U, 0x0ff00ff0U, ALU32Op::XOR),
        aluAutoRow(0x00000001U, 0x0000001fU, ALU32Op::SLL),
        aluAutoRow(0x00000001U, 0x00000020U, ALU32Op::SLL),
        aluAutoRow(0x80000000U, 0x0000001fU, ALU32Op::SRL),
        aluAutoRow(0x80000000U, 0x00000021U, ALU32Op::SRL),
        aluAutoRow(0x80000000U, 0x0000001fU, ALU32Op::SRA),
        aluAutoRow(0x80000000U, 0x00000021U, ALU32Op::SRA),
        aluAutoRow(0xffffffffU, 0x00000001U, ALU32Op::SLT),
        aluAutoRow(0x80000000U, 0x00000000U, ALU32Op::SLT),
        aluAutoRow(0x7fffffffU, 0x80000000U, ALU32Op::SLT),
        aluAutoRow(0xffffffffU, 0x00000001U, ALU32Op::SLTU),
        aluAutoRow(0x00000000U, 0xffffffffU, ALU32Op::SLTU),
        aluAutoRow(0x7fffffffU, 0x80000000U, ALU32Op::SLTU),
        aluAutoRow(0x12345678U, 0x9abcdef0U, ALU32Op::PASS_A),
        aluAutoRow(0x12345678U, 0x9abcdef0U, ALU32Op::PASS_B),
        aluAutoRow(0x12345678U, 0x9abcdef0U, ALU32Op::ZERO),
    };

    for (uint8_t op = 0x0d; op <= 0x1f; ++op) {
        rows.push_back(aluAutoRow(0x12345678U, 0x9abcdef0U, op));
    }
    return rows;
}

circuit::test::ComponentTestSpec alu32TestSpec() {
    circuit::test::ComponentTestSpec spec{
        "ALU32Test",
        std::string(circuit::families::ALU32.id()),
        "ALU32_ROOT",
        {},
        {},
    };
    spec.scenarios.push_back(circuit::test::actionScenarioFromRows(
        "truth-table",
        spec.contract_id,
        alu32Rows(),
        {100'000, 2'000'000}));
    return spec;
}
}

Adder32Test::Adder32Test()
    : TruthTableComponentTest<Adder32>("Adder32Test", "ADDER32_ROOT", adder32Rows()) {}

AddSub32Test::AddSub32Test()
    : TruthTableComponentTest<AddSub32>("AddSub32Test", "ADDSUB32_ROOT", addSub32Rows()) {}

Logic32Test::Logic32Test()
    : TruthTableComponentTest<Logic32>("Logic32Test", "LOGIC32_ROOT", logic32Rows()) {}

ZeroDetect32Test::ZeroDetect32Test()
    : TruthTableComponentTest<ZeroDetect32>("ZeroDetect32Test", "ZERO_DETECT32_ROOT", zeroDetect32Rows()) {}

Comparator32Test::Comparator32Test()
    : TruthTableComponentTest<Comparator32>("Comparator32Test", "COMPARATOR32_ROOT", comparator32Rows()) {}

Shifter32Test::Shifter32Test()
    : TruthTableComponentTest<Shifter32>("Shifter32Test", "SHIFTER32_ROOT", shifter32Rows()) {}

ALU32Test::ALU32Test()
    : ComponentScenarioTest(alu32TestSpec(), "truth-table") {}

std::string ALU32RepresentativeSliceTest::getTestName() const {
    return "ALU32RepresentativeSliceTest";
}

void ALU32RepresentativeSliceTest::verifyResults() {
    expect(
        runRowsBatched<AddSub4Slice>(addSub4Rows(), 100),
        "ALU32RepresentativeSliceTest AddSub4Slice exhaustive");
}
