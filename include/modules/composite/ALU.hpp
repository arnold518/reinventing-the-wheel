#pragma once
#include "modules/composite/WordComponent.hpp"

namespace circuit::word::ALUOp {
inline constexpr uint8_t ADD = 0x00, ADC = 0x01, SUB = 0x02, SBC = 0x03;
inline constexpr uint8_t INC = 0x04, DEC = 0x05, NEG = 0x06;
inline constexpr uint8_t AND = 0x08, OR = 0x09, XOR = 0x0A, NOT = 0x0B;
inline constexpr uint8_t NAND = 0x0C, NOR = 0x0D, XNOR = 0x0E;
inline constexpr uint8_t SLL = 0x10, SRL = 0x11, SRA = 0x12, ROL = 0x13, ROR = 0x14;
inline constexpr uint8_t EQ = 0x15, SLTU = 0x16, SLT = 0x17;
inline constexpr uint8_t MINU = 0x18, MAXU = 0x19, MINS = 0x1A, MAXS = 0x1B;
inline constexpr uint8_t PASS_A = 0x1C, PASS_B = 0x1D, CLEAR = 0x1E;
}

// A, B, OP[5], C_IN -> OUT, ZERO, NEGATIVE, CARRY, OVERFLOW, INVALID_OP.
// Addition uses the parallel-prefix adder.
template<size_t W> class ALU : public circuit::word::WordComponent {
    static_assert(circuit::word::supportedWidth<W>, "ALU supports 8, 16 and 32 bits");
public:
    explicit ALU(std::string name) : WordComponent(std::move(name), circuit::word::Unit::ALU, W) {}
};

namespace circuit::families {
const ComponentFamily& alu(size_t width);
}
