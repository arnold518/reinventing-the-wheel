#include "tests/RV32IALU32Tests.hpp"

#include "modules/composite/ALU32.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/Comparator32.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {
constexpr uint64_t MASK32 = 0xffffffffULL;
constexpr size_t SETTLE_TIME = 1000;

void expect(bool condition, const char* test_name) {
    if (!condition) {
        std::cerr << test_name << " failed" << std::endl;
        assert(false && "RV32I ALU32 test failed");
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

TestRow shifterRow(uint32_t a, uint32_t amount) {
    const uint32_t shamt = amount & 0x1fU;
    return {{{"A", bits(a)}, {"B", bits(amount)}},
            {{"SLL_OUT", bits(a << shamt)},
             {"SRL_OUT", bits(a >> shamt)},
             {"SRA_OUT", bits(sra(a, amount))}}};
}

TestRow aluRow(uint32_t a, uint32_t b, uint8_t op, uint32_t out, bool carry = false, bool overflow = false) {
    return {{{"A", bits(a)}, {"B", bits(b)}, {"OP", bits(op)}},
            {{"OUT", bits(out)},
             {"ZERO", bit(out == 0)},
             {"EQ", bit(a == b)},
             {"LT_SIGNED", bit(static_cast<int32_t>(a) < static_cast<int32_t>(b))},
             {"LT_UNSIGNED", bit(a < b)},
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
}

std::string RV32IALU32Test::getTestName() const {
    return "RV32IALU32Test";
}

void RV32IALU32Test::verifyResults() {
    expect(runRowsBatched<AddSub32>({
        addSubRow(0x00000000U, 0x00000000U, false),
        addSubRow(0x00000001U, 0x00000002U, false),
        addSubRow(0xffffffffU, 0x00000001U, false),
        addSubRow(0x7fffffffU, 0x00000001U, false),
        addSubRow(0x80000000U, 0x80000000U, false),
        addSubRow(0x7fffffffU, 0xffffffffU, false),
        addSubRow(0x00000005U, 0x00000003U, true),
        addSubRow(0x00000000U, 0x00000001U, true),
        addSubRow(0x80000000U, 0x00000001U, true),
        addSubRow(0x7fffffffU, 0xffffffffU, true),
        addSubRow(0x80000000U, 0x7fffffffU, true),
        addSubRow(0xffffffffU, 0xffffffffU, true),
    }, SETTLE_TIME), "RV32IALU32Test AddSub32");

    expect(runRowsBatched<Logic32>({
        {{{"A", bits(0xf0f0f0f0U)}, {"B", bits(0x0ff00ff0U)}},
         {{"AND_OUT", bits(0x00f000f0U)}, {"OR_OUT", bits(0xfff0fff0U)}, {"XOR_OUT", bits(0xff00ff00U)}}},
        {{{"A", bits(0xffffffffU)}, {"B", bits(0x00000000U)}},
         {{"AND_OUT", bits(0x00000000U)}, {"OR_OUT", bits(0xffffffffU)}, {"XOR_OUT", bits(0xffffffffU)}}},
        {{{"A", bits(0xaaaaaaaaU)}, {"B", bits(0x55555555U)}},
         {{"AND_OUT", bits(0x00000000U)}, {"OR_OUT", bits(0xffffffffU)}, {"XOR_OUT", bits(0xffffffffU)}}},
        {{{"A", bits(0x80000000U)}, {"B", bits(0x7fffffffU)}},
         {{"AND_OUT", bits(0x00000000U)}, {"OR_OUT", bits(0xffffffffU)}, {"XOR_OUT", bits(0xffffffffU)}}},
    }, SETTLE_TIME), "RV32IALU32Test Logic32");

    expect(runRowsBatched<ZeroDetect32>({
        {{{"A", bits(0x00000000U)}}, {{"ZERO", bit(true)}}},
        {{{"A", bits(0x00000001U)}}, {{"ZERO", bit(false)}}},
        {{{"A", bits(0x80000000U)}}, {{"ZERO", bit(false)}}},
        {{{"A", bits(0xffffffffU)}}, {{"ZERO", bit(false)}}},
    }, SETTLE_TIME), "RV32IALU32Test ZeroDetect32");

    expect(runRowsBatched<Comparator32>({
        comparatorRow(0x00000000U, 0x00000000U),
        comparatorRow(0x00000001U, 0x00000002U),
        comparatorRow(0xffffffffU, 0x00000001U),
        comparatorRow(0x80000000U, 0x00000000U),
        comparatorRow(0x7fffffffU, 0x80000000U),
        comparatorRow(0x80000000U, 0x7fffffffU),
        comparatorRow(0x00000000U, 0xffffffffU),
        comparatorRow(0xffffffffU, 0xffffffffU),
    }, SETTLE_TIME), "RV32IALU32Test Comparator32");

    std::vector<TestRow> shifter_rows;
    for (uint32_t amount : {0U, 1U, 4U, 8U, 16U, 31U, 32U, 33U, 63U, 0xffffffffU}) {
        shifter_rows.push_back(shifterRow(0x80000001U, amount));
        shifter_rows.push_back(shifterRow(0x7fffffffU, amount));
        shifter_rows.push_back(shifterRow(0xffffffffU, amount));
        shifter_rows.push_back(shifterRow(0x00000001U, amount));
    }
    expect(runRowsBatched<Shifter32>(shifter_rows, SETTLE_TIME), "RV32IALU32Test Shifter32");

    std::vector<TestRow> alu_rows{
        aluAutoRow(0x00000001U, 0x00000002U, ALU32Op::ADD),
        aluAutoRow(0xffffffffU, 0x00000001U, ALU32Op::ADD),
        aluAutoRow(0x7fffffffU, 0x00000001U, ALU32Op::ADD),
        aluAutoRow(0x80000000U, 0x80000000U, ALU32Op::ADD),
        aluAutoRow(0x00000005U, 0x00000005U, ALU32Op::SUB),
        aluAutoRow(0x00000000U, 0x00000001U, ALU32Op::SUB),
        aluAutoRow(0x80000000U, 0x7fffffffU, ALU32Op::SUB),
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
        aluAutoRow(0xffffffffU, 0x00000001U, ALU32Op::SLTU),
        aluAutoRow(0x00000000U, 0xffffffffU, ALU32Op::SLTU),
        aluAutoRow(0x12345678U, 0x9abcdef0U, ALU32Op::PASS_A),
        aluAutoRow(0x12345678U, 0x9abcdef0U, ALU32Op::PASS_B),
        aluAutoRow(0x12345678U, 0x9abcdef0U, ALU32Op::ZERO),
        aluAutoRow(0x12345678U, 0x9abcdef0U, 0x1f),
    };
    expect(runRowsBatched<ALU32>(alu_rows, SETTLE_TIME), "RV32IALU32Test ALU32");
}
