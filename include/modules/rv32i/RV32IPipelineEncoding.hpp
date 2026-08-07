#pragma once

#include "modules/rv32i/RV32IComponentEncoding.hpp"
#include "rv32i/RV32IControl.hpp"
#include <cstdint>

namespace rv32i::pipeline_encoding {

constexpr uint32_t bit(bool value, unsigned offset) {
    return (value ? 1U : 0U) << offset;
}

constexpr uint32_t field(uint32_t value, unsigned offset, uint32_t mask) {
    return (value & mask) << offset;
}

constexpr uint32_t packControl(const RV32IControlSignals& control) {
    return bit(control.legal, 0)
        | bit(control.reg_write, 1)
        | bit(control.mem_read, 2)
        | bit(control.mem_write, 3)
        | bit(control.load_sign_extend, 4)
        | bit(control.halt, 5)
        | bit(control.trap, 6)
        | bit(control.uses_rs1, 7)
        | bit(control.uses_rs2, 8)
        | field(component_encoding::writeback(control.writeback), 9, 0x3U)
        | field(component_encoding::memorySize(control.mem_size), 11, 0x3U)
        | field(component_encoding::branch(control.branch), 13, 0x7U)
        | field(component_encoding::jump(control.jump), 16, 0x3U)
        | field(component_encoding::decodeTrapCause(control.trap_cause), 18, 0xFU)
        | field(control.alu_op, 22, 0x1FU)
        | field(component_encoding::aluSourceA(control.alu_a), 27, 0x3U)
        | field(component_encoding::aluSourceB(control.alu_b), 29, 0x3U);
}

constexpr uint32_t bits(uint32_t word, unsigned offset, uint32_t mask) {
    return (word >> offset) & mask;
}

constexpr RV32IALUSourceA aluSourceA(uint32_t value) {
    switch (value & 0x3U) {
        case 0: return RV32IALUSourceA::RS1;
        case 1: return RV32IALUSourceA::PC;
        default: return RV32IALUSourceA::Zero;
    }
}

constexpr RV32IALUSourceB aluSourceB(uint32_t value) {
    switch (value & 0x3U) {
        case 0: return RV32IALUSourceB::RS2;
        case 1: return RV32IALUSourceB::Immediate;
        default: return RV32IALUSourceB::Zero;
    }
}

constexpr RV32IWritebackSource writebackSource(uint32_t value) {
    switch (value & 0x3U) {
        case 1: return RV32IWritebackSource::ALU;
        case 2: return RV32IWritebackSource::Memory;
        case 3: return RV32IWritebackSource::PCPlus4;
        default: return RV32IWritebackSource::None;
    }
}

constexpr RV32IMemorySize memorySize(uint32_t value) {
    switch (value & 0x3U) {
        case 0: return RV32IMemorySize::Byte;
        case 1: return RV32IMemorySize::Halfword;
        case 2: return RV32IMemorySize::Word;
        default: return RV32IMemorySize::None;
    }
}

constexpr RV32IBranchType branchType(uint32_t value) {
    return value <= static_cast<uint32_t>(RV32IBranchType::BGEU)
        ? static_cast<RV32IBranchType>(value)
        : RV32IBranchType::None;
}

constexpr RV32IJumpType jumpType(uint32_t value) {
    return value <= static_cast<uint32_t>(RV32IJumpType::JALR)
        ? static_cast<RV32IJumpType>(value)
        : RV32IJumpType::None;
}

constexpr RV32ITrapCause trapCause(uint32_t value) {
    return value <= static_cast<uint32_t>(RV32ITrapCause::EnvironmentCall)
        ? static_cast<RV32ITrapCause>(value)
        : RV32ITrapCause::IllegalInstruction;
}

constexpr RV32IControlSignals unpackControl(uint32_t word) {
    RV32IControlSignals control;
    control.legal = bits(word, 0, 1) != 0;
    control.reg_write = bits(word, 1, 1) != 0;
    control.mem_read = bits(word, 2, 1) != 0;
    control.mem_write = bits(word, 3, 1) != 0;
    control.load_sign_extend = bits(word, 4, 1) != 0;
    control.halt = bits(word, 5, 1) != 0;
    control.trap = bits(word, 6, 1) != 0;
    control.uses_rs1 = bits(word, 7, 1) != 0;
    control.uses_rs2 = bits(word, 8, 1) != 0;
    control.writeback = writebackSource(bits(word, 9, 0x3U));
    control.mem_size = memorySize(bits(word, 11, 0x3U));
    control.branch = branchType(bits(word, 13, 0x7U));
    control.jump = jumpType(bits(word, 16, 0x3U));
    control.trap_cause = trapCause(bits(word, 18, 0xFU));
    control.alu_op = static_cast<uint8_t>(bits(word, 22, 0x1FU));
    control.alu_a = aluSourceA(bits(word, 27, 0x3U));
    control.alu_b = aluSourceB(bits(word, 29, 0x3U));
    return control;
}

constexpr uint32_t packAddresses(uint8_t rs1, uint8_t rs2, uint8_t rd) {
    return field(rs1, 0, 0x1FU)
        | field(rs2, 5, 0x1FU)
        | field(rd, 10, 0x1FU);
}

constexpr uint8_t rs1(uint32_t addresses) {
    return static_cast<uint8_t>(bits(addresses, 0, 0x1FU));
}

constexpr uint8_t rs2(uint32_t addresses) {
    return static_cast<uint8_t>(bits(addresses, 5, 0x1FU));
}

constexpr uint8_t rd(uint32_t addresses) {
    return static_cast<uint8_t>(bits(addresses, 10, 0x1FU));
}

constexpr bool statusBit(uint32_t status, unsigned bit_index) {
    return bits(status, bit_index, 1U) != 0;
}

} // namespace rv32i::pipeline_encoding
