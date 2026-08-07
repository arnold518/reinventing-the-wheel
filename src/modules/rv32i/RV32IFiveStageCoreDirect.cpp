#include "modules/rv32i/RV32IFiveStageCoreDirect.hpp"

#include "modules/composite/ALU32.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include <algorithm>
#include <cstdint>
#include <utility>

namespace {
bool high(LogicValue value) {
    return value == LogicValue::HIGH;
}

uint32_t inputWord(const BasicComponent& component, const char* name) {
    return static_cast<uint32_t>(
        component.getInputPin<32>(name)->getValueAsUInt64());
}

uint8_t memorySizeEncoding(rv32i::RV32IMemorySize size) {
    switch (size) {
        case rv32i::RV32IMemorySize::Byte: return 0;
        case rv32i::RV32IMemorySize::Halfword: return 1;
        case rv32i::RV32IMemorySize::Word: return 2;
        case rv32i::RV32IMemorySize::None: return 2;
    }
    return 2;
}

bool isMisaligned(uint32_t address, rv32i::RV32IMemorySize size) {
    switch (size) {
        case rv32i::RV32IMemorySize::Byte:
            return false;
        case rv32i::RV32IMemorySize::Halfword:
            return (address & 0x1U) != 0;
        case rv32i::RV32IMemorySize::Word:
            return (address & 0x3U) != 0;
        case rv32i::RV32IMemorySize::None:
            return true;
    }
    return true;
}

rv32i::RV32IExecutionTrapCause decodeTrap(
    rv32i::RV32ITrapCause cause) {
    switch (cause) {
        case rv32i::RV32ITrapCause::None:
            return rv32i::RV32IExecutionTrapCause::None;
        case rv32i::RV32ITrapCause::IllegalInstruction:
            return rv32i::RV32IExecutionTrapCause::IllegalInstruction;
        case rv32i::RV32ITrapCause::EnvironmentCall:
            return rv32i::RV32IExecutionTrapCause::EnvironmentCall;
    }
    return rv32i::RV32IExecutionTrapCause::IllegalInstruction;
}

uint32_t evaluateAlu(uint8_t operation, uint32_t a, uint32_t b) {
    switch (operation) {
        case ALU32Op::ADD: return a + b;
        case ALU32Op::SUB: return a - b;
        case ALU32Op::AND: return a & b;
        case ALU32Op::OR: return a | b;
        case ALU32Op::XOR: return a ^ b;
        case ALU32Op::SLL: return a << (b & 0x1FU);
        case ALU32Op::SRL: return a >> (b & 0x1FU);
        case ALU32Op::SRA:
            return static_cast<uint32_t>(
                static_cast<int32_t>(a) >> (b & 0x1FU));
        case ALU32Op::SLT:
            return static_cast<int32_t>(a)
                       < static_cast<int32_t>(b)
                ? 1U
                : 0U;
        case ALU32Op::SLTU: return a < b ? 1U : 0U;
        case ALU32Op::PASS_A: return a;
        case ALU32Op::PASS_B: return b;
        case ALU32Op::ZERO: return 0;
        default: return 0;
    }
}

bool branchTaken(
    rv32i::RV32IBranchType branch,
    uint32_t left,
    uint32_t right) {
    switch (branch) {
        case rv32i::RV32IBranchType::None: return false;
        case rv32i::RV32IBranchType::BEQ: return left == right;
        case rv32i::RV32IBranchType::BNE: return left != right;
        case rv32i::RV32IBranchType::BLT:
            return static_cast<int32_t>(left)
                < static_cast<int32_t>(right);
        case rv32i::RV32IBranchType::BGE:
            return static_cast<int32_t>(left)
                >= static_cast<int32_t>(right);
        case rv32i::RV32IBranchType::BLTU: return left < right;
        case rv32i::RV32IBranchType::BGEU: return left >= right;
    }
    return false;
}

bool preMemoryTerminal(
    bool pc_misaligned,
    bool instruction_fault,
    bool target_misaligned,
    const rv32i::RV32IControlSignals& control) {
    return pc_misaligned
        || instruction_fault
        || !control.legal
        || control.trap
        || control.halt
        || target_misaligned;
}
} // namespace

RV32IFiveStageCoreDirect::RV32IFiveStageCoreDirect(std::string name)
    : BasicComponent(
          std::move(name),
          1,
          circuit::families::RV32IFiveStageCore.pinInitializer()) {}

void RV32IFiveStageCoreDirect::resetState() {
    registers_.fill(0);
    fetch_pc_ = 0;
    committed_pc_ = 0;
    halted_ = false;
    trapped_ = false;
    trap_cause_ = rv32i::RV32IExecutionTrapCause::None;
    retired_count_ = 0;
    retired_pc_ = 0;
    retired_instruction_ = 0;
    retired_memory_ = {};
    if_id_ = {};
    id_ex_ = {};
    ex_mem_ = {};
    mem_wb_ = {};
}

RV32IFiveStageCoreDirect::DecodeExecuteState
RV32IFiveStageCoreDirect::decode(
    const FetchDecodeState& state,
    const std::array<uint32_t, 32>& register_snapshot) const {
    DecodeExecuteState result;
    result.valid = state.valid;
    result.pc = state.pc;
    result.instruction = state.instruction;
    result.pc_misaligned = state.pc_misaligned;
    result.instruction_fault = state.instruction_fault;
    if (!state.valid) {
        return result;
    }

    const auto decoded =
        rv32i::RV32IDecoder::decode(state.instruction);
    result.control =
        rv32i::RV32IControl::fromDecoded(decoded);
    result.rs1 = decoded.rs1;
    result.rs2 = decoded.rs2;
    result.rd = decoded.rd;
    result.immediate =
        static_cast<uint32_t>(decoded.immediate);
    result.rs1_value = result.rs1 == 0
        ? 0
        : register_snapshot[result.rs1];
    result.rs2_value = result.rs2 == 0
        ? 0
        : register_snapshot[result.rs2];
    return result;
}

uint32_t RV32IFiveStageCoreDirect::writebackValue(
    const MemoryWritebackState& state) const {
    switch (state.control.writeback) {
        case rv32i::RV32IWritebackSource::None:
            return 0;
        case rv32i::RV32IWritebackSource::ALU:
            return state.alu_result;
        case rv32i::RV32IWritebackSource::Memory:
            return state.memory_data;
        case rv32i::RV32IWritebackSource::PCPlus4:
            return state.pc_plus_4;
    }
    return 0;
}

RV32IFiveStageCoreDirect::ExecuteMemoryState
RV32IFiveStageCoreDirect::execute(
    const DecodeExecuteState& state) const {
    ExecuteMemoryState result;
    result.valid = state.valid;
    result.pc = state.pc;
    result.instruction = state.instruction;
    result.rd = state.rd;
    result.control = state.control;
    result.pc_misaligned = state.pc_misaligned;
    result.instruction_fault = state.instruction_fault;
    if (!state.valid) {
        return result;
    }

    const auto forward = [&](uint8_t source, uint32_t original) {
        if (source == 0) {
            return uint32_t{0};
        }
        if (ex_mem_.valid
            && ex_mem_.control.reg_write
            && ex_mem_.rd != 0
            && ex_mem_.rd == source) {
            if (!ex_mem_.control.mem_read) {
                return ex_mem_.control.writeback
                        == rv32i::RV32IWritebackSource::PCPlus4
                    ? ex_mem_.pc_plus_4
                    : ex_mem_.alu_result;
            }
            return original;
        }
        if (mem_wb_.valid
            && mem_wb_.control.reg_write
            && mem_wb_.rd != 0
            && mem_wb_.rd == source) {
            return writebackValue(mem_wb_);
        }
        return original;
    };

    const auto rs1 =
        forward(state.rs1, state.rs1_value);
    const auto rs2 =
        forward(state.rs2, state.rs2_value);
    const auto alu_a = [&] {
        switch (state.control.alu_a) {
            case rv32i::RV32IALUSourceA::RS1: return rs1;
            case rv32i::RV32IALUSourceA::PC: return state.pc;
            case rv32i::RV32IALUSourceA::Zero: return uint32_t{0};
        }
        return uint32_t{0};
    }();
    const auto alu_b = [&] {
        switch (state.control.alu_b) {
            case rv32i::RV32IALUSourceB::RS2: return rs2;
            case rv32i::RV32IALUSourceB::Immediate:
                return state.immediate;
            case rv32i::RV32IALUSourceB::Zero: return uint32_t{0};
        }
        return uint32_t{0};
    }();

    result.alu_result =
        evaluateAlu(state.control.alu_op, alu_a, alu_b);
    result.store_data = rs2;
    result.pc_plus_4 = state.pc + 4U;
    result.next_pc = result.pc_plus_4;

    const bool branch = branchTaken(
        state.control.branch, rs1, rs2);
    if (branch
        || state.control.jump == rv32i::RV32IJumpType::JAL) {
        result.next_pc = state.pc + state.immediate;
    } else if (
        state.control.jump == rv32i::RV32IJumpType::JALR) {
        result.next_pc =
            (rs1 + state.immediate) & ~uint32_t{1};
    }
    const bool redirect =
        branch
        || state.control.jump != rv32i::RV32IJumpType::None;
    result.target_misaligned =
        redirect && (result.next_pc & 0x3U) != 0;
    return result;
}

RV32IFiveStageCoreDirect::MemoryWritebackState
RV32IFiveStageCoreDirect::accessMemory(
    const ExecuteMemoryState& state) const {
    MemoryWritebackState result;
    result.valid = state.valid;
    result.pc = state.pc;
    result.instruction = state.instruction;
    result.alu_result = state.alu_result;
    result.store_data = state.store_data;
    result.pc_plus_4 = state.pc_plus_4;
    result.next_pc = state.next_pc;
    result.rd = state.rd;
    result.control = state.control;
    result.pc_misaligned = state.pc_misaligned;
    result.instruction_fault = state.instruction_fault;
    result.target_misaligned = state.target_misaligned;
    if (state.valid
        && (state.control.mem_read || state.control.mem_write)) {
        result.memory_data = inputWord(*this, "DMEM_READ_DATA");
        result.data_misaligned =
            isMisaligned(state.alu_result, state.control.mem_size);
        result.data_fault =
            high(getInputValue("DMEM_FAULT"))
            && !result.data_misaligned;
    }
    return result;
}

rv32i::RV32IExecutionTrapCause
RV32IFiveStageCoreDirect::retirementTrap(
    const MemoryWritebackState& state) const {
    using Cause = rv32i::RV32IExecutionTrapCause;
    if (!state.valid) return Cause::None;
    if (state.pc_misaligned) {
        return Cause::InstructionAddressMisaligned;
    }
    if (state.instruction_fault) {
        return Cause::InstructionAccessFault;
    }
    if (!state.control.legal) {
        return Cause::IllegalInstruction;
    }
    if (state.control.trap) {
        return decodeTrap(state.control.trap_cause);
    }

    if (state.data_misaligned) {
        return state.control.mem_write
            ? Cause::StoreAddressMisaligned
            : Cause::LoadAddressMisaligned;
    }
    if (state.data_fault) {
        return state.control.mem_write
            ? Cause::StoreAccessFault
            : Cause::LoadAccessFault;
    }
    if (state.target_misaligned) {
        return Cause::InstructionAddressMisaligned;
    }
    return Cause::None;
}

bool RV32IFiveStageCoreDirect::loadUseHazard() const {
    if (!if_id_.valid
        || !id_ex_.valid
        || !id_ex_.control.mem_read
        || id_ex_.rd == 0) {
        return false;
    }
    const auto decoded =
        rv32i::RV32IDecoder::decode(if_id_.instruction);
    const auto control =
        rv32i::RV32IControl::fromDecoded(decoded);
    return (control.uses_rs1 && decoded.rs1 == id_ex_.rd)
        || (control.uses_rs2 && decoded.rs2 == id_ex_.rd);
}

bool RV32IFiveStageCoreDirect::terminalPending() const {
    const auto decode_terminal = [](const auto& state) {
        return state.valid
            && (state.pc_misaligned
                || state.instruction_fault
                || !state.control.legal
                || state.control.trap
                || state.control.halt);
    };
    if (if_id_.valid) {
        const auto decoded =
            rv32i::RV32IDecoder::decode(if_id_.instruction);
        const auto control =
            rv32i::RV32IControl::fromDecoded(decoded);
        if (if_id_.pc_misaligned
            || if_id_.instruction_fault
            || !control.legal
            || control.trap
            || control.halt) {
            return true;
        }
    }
    return decode_terminal(id_ex_)
        || (decode_terminal(ex_mem_)
            || (ex_mem_.valid && ex_mem_.target_misaligned))
        || (decode_terminal(mem_wb_)
            || (mem_wb_.valid
                && (mem_wb_.target_misaligned
                    || mem_wb_.data_misaligned
                    || mem_wb_.data_fault)));
}

bool RV32IFiveStageCoreDirect::storeWriteActive() const {
    const bool older_terminal =
        mem_wb_.valid
        && (mem_wb_.control.halt
            || retirementTrap(mem_wb_)
                != rv32i::RV32IExecutionTrapCause::None);
    return high(getInputValue("ENABLE"))
        && !halted_
        && !trapped_
        && !older_terminal
        && ex_mem_.valid
        && ex_mem_.control.mem_write
        && !preMemoryTerminal(
            ex_mem_.pc_misaligned,
            ex_mem_.instruction_fault,
            ex_mem_.target_misaligned,
            ex_mem_.control)
        && !isMisaligned(
            ex_mem_.alu_result, ex_mem_.control.mem_size);
}

bool RV32IFiveStageCoreDirect::loadReadActive() const {
    const bool older_terminal =
        mem_wb_.valid
        && (mem_wb_.control.halt
            || retirementTrap(mem_wb_)
                != rv32i::RV32IExecutionTrapCause::None);
    return high(getInputValue("ENABLE"))
        && !halted_
        && !trapped_
        && !older_terminal
        && ex_mem_.valid
        && ex_mem_.control.mem_read
        && !preMemoryTerminal(
            ex_mem_.pc_misaligned,
            ex_mem_.instruction_fault,
            ex_mem_.target_misaligned,
            ex_mem_.control);
}

void RV32IFiveStageCoreDirect::advanceOneCycle() {
    if (!high(getInputValue("ENABLE")) || halted_ || trapped_) {
        return;
    }

    const bool store_active = storeWriteActive();
    const bool load_active = loadReadActive();
    const bool memory_wait =
        (store_active || load_active)
        && !high(getInputValue("DMEM_READY"));
    if (memory_wait) {
        return;
    }

    bool terminal_retired = false;
    if (mem_wb_.valid) {
        const auto cause = retirementTrap(mem_wb_);
        retired_pc_ = mem_wb_.pc;
        retired_instruction_ = mem_wb_.instruction;
        retired_memory_ = {};
        if (mem_wb_.control.mem_read) {
            retired_memory_.kind =
                rv32i::RV32IMemoryAccessKind::Read;
            retired_memory_.size = mem_wb_.control.mem_size;
            retired_memory_.sign_extend =
                mem_wb_.control.load_sign_extend;
            retired_memory_.address = mem_wb_.alu_result;
            retired_memory_.read_data = mem_wb_.memory_data;
            retired_memory_.fault =
                mem_wb_.data_misaligned || mem_wb_.data_fault;
        } else if (mem_wb_.control.mem_write) {
            retired_memory_.kind =
                rv32i::RV32IMemoryAccessKind::Write;
            retired_memory_.size = mem_wb_.control.mem_size;
            retired_memory_.address = mem_wb_.alu_result;
            retired_memory_.write_data = mem_wb_.store_data;
            retired_memory_.fault =
                mem_wb_.data_misaligned || mem_wb_.data_fault;
        }
        ++retired_count_;

        if (cause != rv32i::RV32IExecutionTrapCause::None) {
            trapped_ = true;
            trap_cause_ = cause;
            terminal_retired = true;
        } else if (mem_wb_.control.halt) {
            halted_ = true;
            terminal_retired = true;
        } else {
            if (mem_wb_.control.reg_write && mem_wb_.rd != 0) {
                registers_[mem_wb_.rd] =
                    writebackValue(mem_wb_);
            }
            committed_pc_ = mem_wb_.next_pc;
        }
        registers_[0] = 0;
    }
    // Model the common register-file timing convention: WB writes in the
    // first half of the cycle and ID reads the updated value in the second
    // half. This closes the WB-to-ID dependency without an extra stall.
    const auto register_snapshot = registers_;

    if (terminal_retired) {
        if_id_ = {};
        id_ex_ = {};
        ex_mem_ = {};
        mem_wb_ = {};
        return;
    }

    const auto next_mem_wb = accessMemory(ex_mem_);
    const auto memory_cause = retirementTrap(next_mem_wb);
    const bool older_memory_fault =
        next_mem_wb.valid
        && (next_mem_wb.control.mem_read
            || next_mem_wb.control.mem_write)
        && memory_cause
            != rv32i::RV32IExecutionTrapCause::None;

    auto next_ex_mem = execute(id_ex_);
    const bool ex_preterminal =
        next_ex_mem.valid
        && preMemoryTerminal(
            next_ex_mem.pc_misaligned,
            next_ex_mem.instruction_fault,
            next_ex_mem.target_misaligned,
            next_ex_mem.control);
    const bool ex_redirect =
        next_ex_mem.valid
        && !ex_preterminal
        && (next_ex_mem.control.branch
                != rv32i::RV32IBranchType::None
            ? next_ex_mem.next_pc
                != next_ex_mem.pc_plus_4
            : next_ex_mem.control.jump
                != rv32i::RV32IJumpType::None);
    const bool hazard = loadUseHazard();
    const bool kill_younger =
        older_memory_fault || ex_preterminal || ex_redirect;

    DecodeExecuteState next_id_ex;
    if (!kill_younger && !hazard) {
        next_id_ex = decode(if_id_, register_snapshot);
    }

    FetchDecodeState next_if_id;
    uint32_t next_fetch_pc = fetch_pc_;
    if (kill_younger) {
        if (ex_redirect) {
            next_fetch_pc = next_ex_mem.next_pc;
        }
    } else if (hazard) {
        next_if_id = if_id_;
    } else {
        bool terminal_in_id = false;
        if (if_id_.valid) {
            const auto decoded =
                rv32i::RV32IDecoder::decode(if_id_.instruction);
            const auto control =
                rv32i::RV32IControl::fromDecoded(decoded);
            terminal_in_id =
                if_id_.pc_misaligned
                || if_id_.instruction_fault
                || !control.legal
                || control.trap
                || control.halt;
        }
        if (!terminalPending() && !terminal_in_id
            && high(getInputValue("IMEM_READY"))) {
            next_if_id.valid = true;
            next_if_id.pc = fetch_pc_;
            next_if_id.instruction =
                inputWord(*this, "IMEM_READ_DATA");
            next_if_id.pc_misaligned =
                (fetch_pc_ & 0x3U) != 0;
            next_if_id.instruction_fault =
                high(getInputValue("IMEM_FAULT"));
            next_fetch_pc = fetch_pc_ + 4U;
        }
    }

    mem_wb_ = next_mem_wb;
    ex_mem_ = older_memory_fault
        ? ExecuteMemoryState{}
        : next_ex_mem;
    id_ex_ = next_id_ex;
    if_id_ = next_if_id;
    fetch_pc_ = next_fetch_pc;
}

void RV32IFiveStageCoreDirect::driveOutputs(
    size_t current_time,
    Simulator& simulator) {
    const bool enabled =
        high(getInputValue("ENABLE")) && !halted_ && !trapped_;
    const bool store_active = storeWriteActive();
    const bool load_active = loadReadActive();
    const bool commit_valid = enabled && mem_wb_.valid;

    _updateOutputWire<32>(
        simulator, "PC", committed_pc_, current_time);
    _updateOutputWire<32>(
        simulator, "FETCH_PC", fetch_pc_, current_time);
    _updateOutputWire(
        simulator,
        "HALTED",
        halted_ ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire(
        simulator,
        "TRAPPED",
        trapped_ ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire<4>(
        simulator,
        "TRAP_CAUSE",
        static_cast<uint8_t>(trap_cause_),
        current_time);
    _updateOutputWire(
        simulator,
        "INSTRUCTION_ATTEMPT",
        commit_valid ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire(
        simulator,
        "COMMIT_VALID",
        commit_valid ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire<32>(
        simulator, "RETIRED_COUNT", retired_count_, current_time);
    _updateOutputWire<32>(
        simulator, "RETIRED_PC", retired_pc_, current_time);
    _updateOutputWire<32>(
        simulator,
        "RETIRED_INSTRUCTION",
        retired_instruction_,
        current_time);
    _updateOutputWire(
        simulator,
        "RETIRED_MEM_READ",
        retired_memory_.kind == rv32i::RV32IMemoryAccessKind::Read
            ? LogicValue::HIGH
            : LogicValue::LOW,
        current_time);
    _updateOutputWire(
        simulator,
        "RETIRED_MEM_WRITE",
        retired_memory_.kind == rv32i::RV32IMemoryAccessKind::Write
            ? LogicValue::HIGH
            : LogicValue::LOW,
        current_time);
    _updateOutputWire<32>(
        simulator,
        "RETIRED_MEM_ADDR",
        retired_memory_.address,
        current_time);
    _updateOutputWire<32>(
        simulator,
        "RETIRED_MEM_WRITE_DATA",
        retired_memory_.write_data,
        current_time);
    _updateOutputWire<32>(
        simulator,
        "RETIRED_MEM_READ_DATA",
        retired_memory_.read_data,
        current_time);
    _updateOutputWire<2>(
        simulator,
        "RETIRED_MEM_SIZE",
        retired_memory_.kind == rv32i::RV32IMemoryAccessKind::None
            ? 0
            : memorySizeEncoding(retired_memory_.size),
        current_time);
    _updateOutputWire(
        simulator,
        "RETIRED_MEM_SIGN_EXTEND",
        retired_memory_.sign_extend
            ? LogicValue::HIGH
            : LogicValue::LOW,
        current_time);
    _updateOutputWire(
        simulator,
        "RETIRED_MEM_FAULT",
        retired_memory_.fault
            ? LogicValue::HIGH
            : LogicValue::LOW,
        current_time);

    _updateOutputWire<32>(
        simulator, "IMEM_ADDR", fetch_pc_, current_time);
    _updateOutputWire(
        simulator,
        "IMEM_READ_EN",
        enabled && !terminalPending()
            ? LogicValue::HIGH
            : LogicValue::LOW,
        current_time);

    const auto memory_address = ex_mem_.alu_result;
    const auto memory_size = ex_mem_.control.mem_size;
    _updateOutputWire<32>(
        simulator, "DMEM_ADDR", memory_address, current_time);
    _updateOutputWire<32>(
        simulator,
        "DMEM_WRITE_DATA",
        ex_mem_.store_data,
        current_time);
    _updateOutputWire(
        simulator,
        "DMEM_READ_EN",
        load_active ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire(
        simulator,
        "DMEM_WRITE_EN",
        store_active ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire<2>(
        simulator,
        "DMEM_SIZE",
        (store_active || load_active)
            ? memorySizeEncoding(memory_size)
            : 0,
        current_time);
    _updateOutputWire(
        simulator,
        "DMEM_SIGN_EXTEND",
        load_active && ex_mem_.control.load_sign_extend
            ? LogicValue::HIGH
            : LogicValue::LOW,
        current_time);

    const bool stall =
        loadUseHazard()
        || ((store_active || load_active)
            && !high(getInputValue("DMEM_READY")));
    _updateOutputWire(
        simulator,
        "PIPELINE_STALL",
        stall ? LogicValue::HIGH : LogicValue::LOW,
        current_time);

    const bool memory_stall =
        enabled && (store_active || load_active)
        && !high(getInputValue("DMEM_READY"));
    const bool data_port_stall = false;
    const bool retire_terminal =
        enabled && mem_wb_.valid
        && (mem_wb_.control.halt
            || retirementTrap(mem_wb_)
                != rv32i::RV32IExecutionTrapCause::None);
    const bool normal_step =
        enabled && !memory_stall
        && !retire_terminal;
    const auto next_mem_wb = accessMemory(ex_mem_);
    const bool older_memory_fault =
        next_mem_wb.valid
        && (next_mem_wb.control.mem_read
            || next_mem_wb.control.mem_write)
        && retirementTrap(next_mem_wb)
            != rv32i::RV32IExecutionTrapCause::None;
    const auto next_ex_mem = execute(id_ex_);
    const bool ex_preterminal =
        next_ex_mem.valid
        && preMemoryTerminal(
            next_ex_mem.pc_misaligned,
            next_ex_mem.instruction_fault,
            next_ex_mem.target_misaligned,
            next_ex_mem.control);
    const bool ex_redirect =
        next_ex_mem.valid && !ex_preterminal
        && (next_ex_mem.control.branch
                != rv32i::RV32IBranchType::None
            ? next_ex_mem.next_pc != next_ex_mem.pc_plus_4
            : next_ex_mem.control.jump
                != rv32i::RV32IJumpType::None);
    const bool load_use_stall =
        normal_step && loadUseHazard();
    const bool pipeline_flush =
        normal_step
        && (older_memory_fault || ex_preterminal || ex_redirect);
    for (const auto& [name, active] :
         std::array<std::pair<const char*, bool>, 4>{{
             {"LOAD_USE_STALL", load_use_stall},
             {"MEMORY_STALL", memory_stall},
             {"DATA_PORT_STALL", data_port_stall},
             {"PIPELINE_FLUSH", pipeline_flush},
         }}) {
        _updateOutputWire(
            simulator, name,
            active ? LogicValue::HIGH : LogicValue::LOW,
            current_time);
    }

    _updateOutputWire(
        simulator,
        "IF_ID_VALID",
        if_id_.valid ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire<32>(
        simulator, "IF_ID_PC", if_id_.pc, current_time);
    _updateOutputWire(
        simulator,
        "ID_EX_VALID",
        id_ex_.valid ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire<32>(
        simulator, "ID_EX_PC", id_ex_.pc, current_time);
    _updateOutputWire(
        simulator,
        "EX_MEM_VALID",
        ex_mem_.valid ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire<32>(
        simulator, "EX_MEM_PC", ex_mem_.pc, current_time);
    _updateOutputWire(
        simulator,
        "MEM_WB_VALID",
        mem_wb_.valid ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire<32>(
        simulator, "MEM_WB_PC", mem_wb_.pc, current_time);
}

void RV32IFiveStageCoreDirect::evaluate(
    size_t current_time,
    Simulator& simulator) {
    const auto clk = getInputValue("CLK");
    const auto rst = getInputValue("RST");
    if (rst == LogicValue::HIGH) {
        resetState();
    } else if (
        rst == LogicValue::LOW
        && previous_clk_ == LogicValue::LOW
        && clk == LogicValue::HIGH) {
        advanceOneCycle();
    }
    previous_clk_ = clk;
    driveOutputs(current_time, simulator);
}

rv32i::RV32IArchitecturalState
RV32IFiveStageCoreDirect::snapshotArchitecturalState() const {
    rv32i::RV32IState state;
    state.pc = committed_pc_;
    state.x = registers_;
    state.halted = halted_;
    state.trapped = trapped_;
    state.trap_cause = trap_cause_;
    state.instruction_count = retired_count_;
    return rv32i::RV32IArchitecturalState::fromKnown(state);
}
