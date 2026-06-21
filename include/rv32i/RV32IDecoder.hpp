#pragma once

#include "rv32i/RV32IInstruction.hpp"
#include <cstdint>
#include <string>

namespace rv32i {

class RV32IDecoder {
public:
    static RV32IDecodedInstruction decode(uint32_t raw);

    static uint8_t opcode(uint32_t raw);
    static uint8_t rd(uint32_t raw);
    static uint8_t funct3(uint32_t raw);
    static uint8_t rs1(uint32_t raw);
    static uint8_t rs2(uint32_t raw);
    static uint8_t funct7(uint32_t raw);
    static uint16_t imm12(uint32_t raw);
    static uint8_t shamt(uint32_t raw);

    static int32_t signExtend(uint32_t value, unsigned bit_count);
    static int32_t immediateI(uint32_t raw);
    static int32_t immediateS(uint32_t raw);
    static int32_t immediateB(uint32_t raw);
    static int32_t immediateU(uint32_t raw);
    static int32_t immediateJ(uint32_t raw);
    static std::string disassemble(uint32_t raw, uint32_t pc = 0);
};

}
