#pragma once

#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>

namespace rv32i::component_encoding {

constexpr uint8_t aluSourceA(RV32IALUSourceA source) {
    switch (source) {
        case RV32IALUSourceA::RS1: return 0;
        case RV32IALUSourceA::PC: return 1;
        case RV32IALUSourceA::Zero: return 2;
    }
    return 2;
}

constexpr uint8_t aluSourceB(RV32IALUSourceB source) {
    switch (source) {
        case RV32IALUSourceB::RS2: return 0;
        case RV32IALUSourceB::Immediate: return 1;
        case RV32IALUSourceB::Zero: return 2;
    }
    return 2;
}

constexpr uint8_t writeback(RV32IWritebackSource source) {
    switch (source) {
        case RV32IWritebackSource::None: return 0;
        case RV32IWritebackSource::ALU: return 1;
        case RV32IWritebackSource::Memory: return 2;
        case RV32IWritebackSource::PCPlus4: return 3;
    }
    return 0;
}

// This encoding is the external Memory64Kx32 SIZE contract.
constexpr uint8_t memorySize(RV32IMemorySize size) {
    switch (size) {
        case RV32IMemorySize::Byte: return 0;
        case RV32IMemorySize::Halfword: return 1;
        case RV32IMemorySize::Word: return 2;
        case RV32IMemorySize::None: return 2;
    }
    return 2;
}

constexpr uint8_t branch(RV32IBranchType type) {
    return static_cast<uint8_t>(type);
}

constexpr uint8_t jump(RV32IJumpType type) {
    return static_cast<uint8_t>(type);
}

constexpr uint8_t decodeTrapCause(RV32ITrapCause cause) {
    return static_cast<uint8_t>(cause);
}

constexpr uint8_t executionTrapCause(RV32IExecutionTrapCause cause) {
    return static_cast<uint8_t>(cause);
}

}
