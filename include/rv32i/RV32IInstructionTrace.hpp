#pragma once

#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>

namespace rv32i {

enum class RV32IMemoryAccessKind {
    None,
    Read,
    Write,
};

struct RV32IMemoryTrace {
    RV32IMemoryAccessKind kind = RV32IMemoryAccessKind::None;
    RV32IMemorySize size = RV32IMemorySize::None;
    bool sign_extend = false;
    uint32_t address = 0;
    uint32_t write_data = 0;
    uint32_t read_data = 0;
    bool fault = false;
};

struct RV32IWritebackTrace {
    bool enabled = false;
    uint8_t rd = 0;
    uint32_t value = 0;
    bool ignored_x0 = false;
};

struct RV32IInstructionTrace {
    uint64_t instruction_index = 0;

    uint32_t pc_before = 0;
    uint32_t raw_instruction = 0;
    RV32IDecodedInstruction decoded{};
    RV32IControlSignals control{};

    uint8_t rs1 = 0;
    uint8_t rs2 = 0;
    uint8_t rd = 0;
    uint32_t rs1_value = 0;
    uint32_t rs2_value = 0;
    int32_t immediate = 0;

    uint32_t alu_a = 0;
    uint32_t alu_b = 0;
    uint32_t alu_result = 0;

    RV32IMemoryTrace memory{};
    RV32IWritebackTrace writeback{};

    bool branch_taken = false;
    uint32_t pc_after = 0;

    bool halted = false;
    bool trapped = false;
    RV32IExecutionTrapCause trap_cause = RV32IExecutionTrapCause::None;
};

std::string_view toString(RV32IMemoryAccessKind kind);

}
