#include "modules/composite/ALU8.hpp"
#include <cstdint>

namespace {
LogicValue flag(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}

bool addOverflow(uint8_t a, uint8_t b, uint8_t result) {
    return ((~(a ^ b) & (a ^ result) & 0x80U) != 0);
}

bool subOverflow(uint8_t a, uint8_t b, uint8_t result) {
    return (((a ^ b) & (a ^ result) & 0x80U) != 0);
}
}

ALU8::ALU8(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin<4>("OP", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
        self->addPin("ZERO", PinType::OUTPUT);
        self->addPin("CARRY", PinType::OUTPUT);
        self->addPin("OVERFLOW", PinType::OUTPUT);
        self->addPin("NEGATIVE", PinType::OUTPUT);
    }) {}

void ALU8::evaluate(size_t current_time, Simulator& simulator) {
    auto a = static_cast<uint8_t>(getInputValueAsUInt64("A") & 0xFFU);
    auto b = static_cast<uint8_t>(getInputValueAsUInt64("B") & 0xFFU);
    auto op = static_cast<uint8_t>(getInputValueAsUInt64("OP") & 0x0FU);

    uint8_t result = 0;
    bool carry = false;
    bool overflow = false;

    switch (op) {
        case 0x0: {
            uint16_t sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(b);
            result = static_cast<uint8_t>(sum & 0xFFU);
            carry = sum > 0xFFU;
            overflow = addOverflow(a, b, result);
            break;
        }
        case 0x1: {
            uint16_t sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(~b & 0xFFU) + 1U;
            result = static_cast<uint8_t>(sum & 0xFFU);
            carry = sum > 0xFFU;
            overflow = subOverflow(a, b, result);
            break;
        }
        case 0x2: result = a & b; break;
        case 0x3: result = a | b; break;
        case 0x4: result = a ^ b; break;
        case 0x5: result = static_cast<uint8_t>(~a); break;
        case 0x6:
            result = static_cast<uint8_t>((a << 1U) & 0xFFU);
            carry = (a & 0x80U) != 0;
            break;
        case 0x7:
            result = static_cast<uint8_t>(a >> 1U);
            carry = (a & 0x01U) != 0;
            break;
        case 0x8:
            result = static_cast<uint8_t>((a >> 1U) | (a & 0x80U));
            carry = (a & 0x01U) != 0;
            break;
        case 0x9: {
            uint16_t sum = static_cast<uint16_t>(a) + 1U;
            result = static_cast<uint8_t>(sum & 0xFFU);
            carry = sum > 0xFFU;
            overflow = addOverflow(a, 1U, result);
            break;
        }
        case 0xA:
            result = static_cast<uint8_t>((a - 1U) & 0xFFU);
            carry = a != 0;
            overflow = a == 0x80U;
            break;
        case 0xB:
            result = static_cast<uint8_t>((~a + 1U) & 0xFFU);
            carry = a != 0;
            overflow = a == 0x80U;
            break;
        case 0xC: result = a; break;
        case 0xD: result = b; break;
        case 0xE: {
            uint16_t sum = static_cast<uint16_t>(a) + static_cast<uint16_t>(~b & 0xFFU) + 1U;
            result = static_cast<uint8_t>(sum & 0xFFU);
            carry = sum > 0xFFU;
            overflow = subOverflow(a, b, result);
            break;
        }
        case 0xF:
        default:
            result = 0;
            break;
    }

    _updateOutputWire<8>(simulator, "OUT", result, current_time);
    _updateOutputWire(simulator, "ZERO", flag(result == 0), current_time);
    _updateOutputWire(simulator, "CARRY", flag(carry), current_time);
    _updateOutputWire(simulator, "OVERFLOW", flag(overflow), current_time);
    _updateOutputWire(simulator, "NEGATIVE", flag((result & 0x80U) != 0), current_time);
}
