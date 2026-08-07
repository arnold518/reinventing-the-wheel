#include "modules/composite/ALU32Direct.hpp"
#include "components/selection/ComponentFamily.hpp"

#include "modules/composite/ALU32.hpp"
#include "simulator/Simulator.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace {
bool allKnown(const std::vector<LogicValue>& values) {
    for (const auto value : values) {
        if (value != LogicValue::LOW && value != LogicValue::HIGH) {
            return false;
        }
    }
    return true;
}
LogicValue logic(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}

uint32_t arithmeticShiftRight(uint32_t value, uint32_t amount) {
    amount &= 0x1FU;
    if (amount == 0) {
        return value;
    }
    const uint32_t shifted = value >> amount;
    if ((value & 0x80000000U) == 0) {
        return shifted;
    }
    return shifted | (~uint32_t{0} << (32U - amount));
}

bool addOverflow(uint32_t a, uint32_t b, uint32_t result) {
    return (~(a ^ b) & (a ^ result) & 0x80000000U) != 0;
}

bool subOverflow(uint32_t a, uint32_t b, uint32_t result) {
    return ((a ^ b) & (a ^ result) & 0x80000000U) != 0;
}
}

ALU32Direct::ALU32Direct(std::string name)
    : BasicComponent(std::move(name), 1,
                     circuit::families::ALU32.pinInitializer()) {}

void ALU32Direct::evaluate(size_t current_time, Simulator& simulator) {
    const auto a_bits = getInputPin<32>("A")->getValueAsVector();
    const auto b_bits = getInputPin<32>("B")->getValueAsVector();
    const auto op_bits = getInputPin<5>("OP")->getValueAsVector();
    if (!allKnown(a_bits) || !allKnown(b_bits) || !allKnown(op_bits)) {
        _updateOutputWire<32>(simulator, "OUT", std::vector<LogicValue>(32, LogicValue::UNKNOWN), current_time);
        for (const char* pin : {"ZERO", "EQ", "LT_SIGNED", "LT_UNSIGNED",
                                "NEGATIVE", "CARRY_OUT", "OVERFLOW"}) {
            _updateOutputWire(simulator, pin, LogicValue::UNKNOWN, current_time);
        }
        return;
    }

    const uint32_t a = static_cast<uint32_t>(getInputPin<32>("A")->getValueAsUInt64());
    const uint32_t b = static_cast<uint32_t>(getInputPin<32>("B")->getValueAsUInt64());
    const uint8_t op = static_cast<uint8_t>(getInputPin<5>("OP")->getValueAsUInt64());
    uint32_t result = 0;
    bool carry = false;
    bool overflow = false;

    switch (op) {
        case ALU32Op::ADD: {
            const uint64_t wide = static_cast<uint64_t>(a) + static_cast<uint64_t>(b);
            result = static_cast<uint32_t>(wide);
            carry = (wide >> 32U) != 0;
            overflow = addOverflow(a, b, result);
            break;
        }
        case ALU32Op::SUB:
            result = a - b;
            carry = a >= b;
            overflow = subOverflow(a, b, result);
            break;
        case ALU32Op::AND: result = a & b; break;
        case ALU32Op::OR: result = a | b; break;
        case ALU32Op::XOR: result = a ^ b; break;
        case ALU32Op::SLL: result = a << (b & 0x1FU); break;
        case ALU32Op::SRL: result = a >> (b & 0x1FU); break;
        case ALU32Op::SRA: result = arithmeticShiftRight(a, b); break;
        case ALU32Op::SLT:
            result = static_cast<int32_t>(a) < static_cast<int32_t>(b) ? 1U : 0U;
            carry = a >= b;
            overflow = subOverflow(a, b, a - b);
            break;
        case ALU32Op::SLTU:
            result = a < b ? 1U : 0U;
            carry = a >= b;
            overflow = subOverflow(a, b, a - b);
            break;
        case ALU32Op::PASS_A: result = a; break;
        case ALU32Op::PASS_B: result = b; break;
        case ALU32Op::ZERO: result = 0; break;
        default: result = 0; break;
    }

    _updateOutputWire<32>(simulator, "OUT", result, current_time);
    _updateOutputWire(simulator, "ZERO", logic(result == 0), current_time);
    const bool comparison_valid =
        op == ALU32Op::SUB || op == ALU32Op::SLT
        || op == ALU32Op::SLTU;
    _updateOutputWire(
        simulator,
        "EQ",
        logic(comparison_valid && a == b),
        current_time);
    _updateOutputWire(
        simulator,
        "LT_SIGNED",
        logic(
            comparison_valid
            && static_cast<int32_t>(a) < static_cast<int32_t>(b)),
        current_time);
    _updateOutputWire(
        simulator,
        "LT_UNSIGNED",
        logic(comparison_valid && a < b),
        current_time);
    _updateOutputWire(simulator, "NEGATIVE", logic((result & 0x80000000U) != 0), current_time);
    _updateOutputWire(simulator, "CARRY_OUT", logic(carry), current_time);
    _updateOutputWire(simulator, "OVERFLOW", logic(overflow), current_time);
}
