#pragma once

#include <cstdint>
#include <string_view>

namespace rv32i {

enum class RV32IFormat {
    R,
    I,
    S,
    B,
    U,
    J,
    Invalid,
};

enum class RV32IInstruction {
    ADD,
    SUB,
    SLL,
    SLT,
    SLTU,
    XOR,
    SRL,
    SRA,
    OR,
    AND,

    ADDI,
    SLTI,
    SLTIU,
    XORI,
    ORI,
    ANDI,
    SLLI,
    SRLI,
    SRAI,

    LB,
    LH,
    LW,
    LBU,
    LHU,

    SB,
    SH,
    SW,

    BEQ,
    BNE,
    BLT,
    BGE,
    BLTU,
    BGEU,

    JAL,
    JALR,
    LUI,
    AUIPC,
    FENCE,
    ECALL,
    EBREAK,

    INVALID,
};

enum class RV32IDecodeStatus {
    Legal,
    InvalidInstructionLength,
    UnknownOpcode,
    UnsupportedFunct3,
    UnsupportedFunct7,
    UnsupportedSystem,
    UnsupportedExtension,
};

struct RV32IDecodedInstruction {
    uint32_t raw = 0;
    RV32IInstruction instruction = RV32IInstruction::INVALID;
    RV32IFormat format = RV32IFormat::Invalid;
    RV32IDecodeStatus status = RV32IDecodeStatus::UnknownOpcode;
    bool legal = false;

    uint8_t opcode = 0;
    uint8_t rd = 0;
    uint8_t funct3 = 0;
    uint8_t rs1 = 0;
    uint8_t rs2 = 0;
    uint8_t funct7 = 0;
    uint16_t imm12 = 0;
    uint8_t shamt = 0;

    int32_t immediate = 0;
};

std::string_view toString(RV32IFormat format);
std::string_view toString(RV32IInstruction instruction);
std::string_view toString(RV32IDecodeStatus status);

}
