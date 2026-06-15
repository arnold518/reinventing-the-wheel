#pragma once

#include "rv32i/RV32IDecoder.hpp"
#include <cstdint>
#include <string_view>

namespace rv32i {

enum class RV32IALUSourceA {
    RS1,
    PC,
    Zero,
};

enum class RV32IALUSourceB {
    RS2,
    Immediate,
    Zero,
};

enum class RV32IWritebackSource {
    None,
    ALU,
    Memory,
    PCPlus4,
};

enum class RV32IMemorySize {
    None,
    Byte,
    Halfword,
    Word,
};

enum class RV32IBranchType {
    None,
    BEQ,
    BNE,
    BLT,
    BGE,
    BLTU,
    BGEU,
};

enum class RV32IJumpType {
    None,
    JAL,
    JALR,
};

enum class RV32ITrapCause {
    None,
    IllegalInstruction,
    EnvironmentCall,
};

struct RV32IControlSignals {
    bool legal = false;

    uint8_t alu_op = 0;
    RV32IALUSourceA alu_a = RV32IALUSourceA::Zero;
    RV32IALUSourceB alu_b = RV32IALUSourceB::Zero;

    bool reg_write = false;
    RV32IWritebackSource writeback = RV32IWritebackSource::None;

    bool mem_read = false;
    bool mem_write = false;
    RV32IMemorySize mem_size = RV32IMemorySize::None;
    bool load_sign_extend = false;

    RV32IBranchType branch = RV32IBranchType::None;
    RV32IJumpType jump = RV32IJumpType::None;

    bool uses_rs1 = false;
    bool uses_rs2 = false;
    bool uses_rd = false;
    bool uses_immediate = false;

    bool halt = false;
    bool trap = false;
    RV32ITrapCause trap_cause = RV32ITrapCause::None;
};

class RV32IControl {
public:
    static RV32IControlSignals fromDecoded(const RV32IDecodedInstruction& decoded);
    static RV32IControlSignals fromRaw(uint32_t raw);
};

std::string_view toString(RV32IALUSourceA source);
std::string_view toString(RV32IALUSourceB source);
std::string_view toString(RV32IWritebackSource source);
std::string_view toString(RV32IMemorySize size);
std::string_view toString(RV32IBranchType branch);
std::string_view toString(RV32IJumpType jump);
std::string_view toString(RV32ITrapCause cause);

}
