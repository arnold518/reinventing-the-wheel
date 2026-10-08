#include "modules/rv32i/RV32IPipelineObservation.hpp"
#include "modules/composite/ALU32Behavioral.hpp"
#include "basic/LogicValueHelpers.hpp"
#include "modules/rv32i/RV32IFiveStageCoreBehavioral.hpp"

#include "modules/composite/ALU32.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "RV32IPipelineRules.hpp"
#include <algorithm>
#include <cstdint>
#include <utility>

namespace {
using circuit::logic_values::high;

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

// The stage rules live in RV32IPipelineRules.hpp and are shared with the
// stages' own behavioral forms.
using rv32i::pipeline_rules::dataMisaligned;
} // namespace

RV32IFiveStageCoreBehavioral::RV32IFiveStageCoreBehavioral(std::string name, rv32i::PredictionConfig prediction)
    : BasicComponent(
          std::move(name),
          1,
          circuit::families::RV32IFiveStageCore.pinInitializer()), prediction_(prediction) {}

void RV32IFiveStageCoreBehavioral::resetState() {
    prediction_.reset();
    registers_.fill(0);
    fetch_pc_ = 0;
    halted_ = false;
    trapped_ = false;
    trap_cause_ = rv32i::RV32IExecutionTrapCause::None;
    if_id_ = {};
    id_ex_ = {};
    ex_mem_ = {};
    mem_wb_ = {};
}

RV32IFiveStageCoreBehavioral::DecodeExecuteState
RV32IFiveStageCoreBehavioral::decode(
    const FetchDecodeState& state,
    const std::array<uint32_t, 32>& register_snapshot) const {
    DecodeExecuteState result;
    result.predicted_next_pc = state.predicted_next_pc;
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

uint32_t RV32IFiveStageCoreBehavioral::writebackValue(
    const MemoryWritebackState& state) const {
    return rv32i::pipeline_rules::writebackValue(
        state.control, state.alu_result, state.memory_data, state.pc_plus_4);
}

RV32IFiveStageCoreBehavioral::ExecuteMemoryState
RV32IFiveStageCoreBehavioral::execute(
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

    // A producer in EX/MEM has its value ready unless it is a load, whose
    // data arrives one stage later.
    const rv32i::pipeline_rules::ForwardSource from_ex_mem{
        ex_mem_.valid, ex_mem_.control.reg_write, !ex_mem_.control.mem_read, ex_mem_.rd,
        ex_mem_.control.writeback == rv32i::RV32IWritebackSource::PCPlus4 ? ex_mem_.pc_plus_4 : ex_mem_.alu_result};
    const rv32i::pipeline_rules::ForwardSource from_mem_wb{
        mem_wb_.valid, mem_wb_.control.reg_write, true, mem_wb_.rd, writebackValue(mem_wb_)};
    const auto rs1 = rv32i::pipeline_rules::forwardOperand(state.rs1, state.rs1_value, from_ex_mem, from_mem_wb);
    const auto rs2 = rv32i::pipeline_rules::forwardOperand(state.rs2, state.rs2_value, from_ex_mem, from_mem_wb);
    const auto executed = rv32i::pipeline_rules::execute(state.control, state.pc, state.immediate, rs1, rs2);
    result.alu_result = executed.alu_result;
    result.store_data = rs2;
    result.pc_plus_4 = executed.pc_plus_4;
    result.next_pc = executed.next_pc;
    result.branch_taken = executed.branch_taken;
    result.target_misaligned = executed.target_misaligned;
    return result;
}

RV32IFiveStageCoreBehavioral::MemoryWritebackState
RV32IFiveStageCoreBehavioral::accessMemory(
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
        result.memory_data = rv32i::memoryLoadData(inputWord(*this, "DMEM_RSP_READ_DATA"),state.alu_result,memorySizeEncoding(state.control.mem_size),state.control.load_sign_extend);
        result.data_misaligned =
            dataMisaligned(state.alu_result, state.control.mem_size);
        result.data_fault =
            (getInputPin<3>("DMEM_RSP_STATUS")->getValueAsUInt64()!=0)
            && !result.data_misaligned;
    }
    return result;
}

rv32i::RV32IExecutionTrapCause
RV32IFiveStageCoreBehavioral::retirementTrap(
    const MemoryWritebackState& state) const {
    return rv32i::pipeline_rules::retirementCause(
        state.valid, state.pc_misaligned, state.instruction_fault, state.control,
        state.data_misaligned, state.data_fault, state.target_misaligned);
}

bool RV32IFiveStageCoreBehavioral::loadUseHazard() const {
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

bool RV32IFiveStageCoreBehavioral::terminalPending() const {
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

bool RV32IFiveStageCoreBehavioral::storeWriteActive() const {
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
        && !rv32i::pipeline_rules::preterminal(
            ex_mem_.pc_misaligned,
            ex_mem_.instruction_fault,
            ex_mem_.control,
            ex_mem_.target_misaligned)
        && !dataMisaligned(
            ex_mem_.alu_result, ex_mem_.control.mem_size);
}

bool RV32IFiveStageCoreBehavioral::loadReadActive() const {
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
        && !rv32i::pipeline_rules::preterminal(
            ex_mem_.pc_misaligned,
            ex_mem_.instruction_fault,
            ex_mem_.control,
            ex_mem_.target_misaligned)
        // Alignment is resolved locally before issuing a transaction, just
        // as in the structural memory stage and the store guard above.
        && !dataMisaligned(
            ex_mem_.alu_result, ex_mem_.control.mem_size);
}

void RV32IFiveStageCoreBehavioral::advanceOneCycle() {
    if (!high(getInputValue("ENABLE")) || halted_ || trapped_) {
        return;
    }

    const bool store_active = storeWriteActive();
    const bool load_active = loadReadActive();
    const bool memory_wait =
        (store_active || load_active)
        && !data_request_.done(*this,"DMEM_",ex_mem_.alu_result,storeWriteActive()||loadReadActive());
    if (memory_wait) {
        return;
    }
    const auto predicted_fetch_pc=prediction_.predict(fetch_pc_,inputWord(*this,"IMEM_RSP_READ_DATA"));

    bool terminal_retired = false;
    if (mem_wb_.valid) {
        const auto cause = retirementTrap(mem_wb_);

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
        && rv32i::pipeline_rules::preterminal(
            next_ex_mem.pc_misaligned,
            next_ex_mem.instruction_fault,
            next_ex_mem.control,
            next_ex_mem.target_misaligned);
    const bool ex_redirect =
        next_ex_mem.valid
        && !ex_preterminal
        && next_ex_mem.next_pc != id_ex_.predicted_next_pc;
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
            && instruction_request_.done(*this,"IMEM_",fetch_pc_,high(getInputValue("ENABLE"))&&!halted_&&!trapped_)) {
            next_if_id.valid = true;
            next_if_id.pc = fetch_pc_;
            next_if_id.instruction =
                inputWord(*this, "IMEM_RSP_READ_DATA");
            next_if_id.predicted_next_pc = predicted_fetch_pc;
            next_if_id.pc_misaligned =
                (fetch_pc_ & 0x3U) != 0;
            next_if_id.instruction_fault =
                (getInputPin<3>("IMEM_RSP_STATUS")->getValueAsUInt64()!=0);
            next_fetch_pc = predicted_fetch_pc;
        }
    }

    if (!older_memory_fault && !ex_preterminal && next_ex_mem.valid
        && next_ex_mem.control.branch != rv32i::RV32IBranchType::None)
        prediction_.update(next_ex_mem.pc,next_ex_mem.branch_taken);
    mem_wb_ = next_mem_wb;
    ex_mem_ = older_memory_fault
        ? ExecuteMemoryState{}
        : next_ex_mem;
    id_ex_ = next_id_ex;
    if_id_ = next_if_id;
    fetch_pc_ = next_fetch_pc;
}

void RV32IFiveStageCoreBehavioral::driveOutputs(
    size_t current_time,
    Simulator& simulator) {
    const bool enabled =
        high(getInputValue("ENABLE")) && !halted_ && !trapped_;
    const bool store_active = storeWriteActive();
    const bool load_active = loadReadActive();
    // Match the coordinator: a held MEM/WB packet must not retire again
    // while an older cycle is frozen waiting for the data port.
    const bool memory_wait = (store_active || load_active)
        && !data_request_.done(*this,"DMEM_",ex_mem_.alu_result,storeWriteActive()||loadReadActive());
    const bool commit_valid = enabled && getInputValue("RST") == LogicValue::LOW
        && !memory_wait && mem_wb_.valid;

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
    const bool stall =
        loadUseHazard()
        || ((store_active || load_active)
            && !data_request_.done(*this,"DMEM_",ex_mem_.alu_result,storeWriteActive()||loadReadActive()));
    _updateOutputWire(
        simulator,
        "PIPELINE_STALL",
        stall ? LogicValue::HIGH : LogicValue::LOW,
        current_time);

    const bool memory_stall =
        enabled && (store_active || load_active)
        && !data_request_.done(*this,"DMEM_",ex_mem_.alu_result,storeWriteActive()||loadReadActive());
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
        && rv32i::pipeline_rules::preterminal(
            next_ex_mem.pc_misaligned,
            next_ex_mem.instruction_fault,
            next_ex_mem.control,
            next_ex_mem.target_misaligned);
    const bool ex_redirect =
        next_ex_mem.valid && !ex_preterminal
        && next_ex_mem.next_pc != id_ex_.predicted_next_pc;
    const bool load_use_stall =
        normal_step && loadUseHazard();
    const bool pipeline_flush =
        normal_step
        && (older_memory_fault || ex_preterminal || ex_redirect);
    const bool take_instruction=normal_step && !pipeline_flush && !load_use_stall && !terminalPending()
        && instruction_request_.done(*this,"IMEM_",fetch_pc_,high(getInputValue("ENABLE"))&&!halted_&&!trapped_);
    const bool take_data=normal_step || (retire_terminal && !memory_stall);
    const bool reset=getInputValue("RST")!=LogicValue::LOW;
    const unsigned port_size=(store_active||load_active)?memorySizeEncoding(ex_mem_.control.mem_size):0;
    const bool discard_instruction=pipeline_flush||(enabled&&!memory_stall&&retire_terminal)||halted_||trapped_;
    rv32i::publishMemoryRequest(instruction_request_,reset,take_instruction||discard_instruction,fetch_pc_,
        enabled,0,fetch_pc_,0,15,
        [&]<size_t W>(const char* pin,uint64_t value){ _updateOutputWire<W>(simulator,std::string("IMEM_")+pin,value,current_time); });
    rv32i::publishMemoryRequest(data_request_,reset,take_data,ex_mem_.alu_result,
        store_active||load_active,store_active?1:0,rv32i::memoryWordAddress(ex_mem_.alu_result,port_size),
        ex_mem_.store_data<<((ex_mem_.alu_result&3)*8),rv32i::memoryWriteMask(ex_mem_.alu_result,port_size),
        [&]<size_t W>(const char* pin,uint64_t value){ _updateOutputWire<W>(simulator,std::string("DMEM_")+pin,value,current_time); });
    for (const auto& [name, active] :
         std::array<std::pair<const char*, bool>, 7>{{
             {"LOAD_USE_STALL", load_use_stall},
             {"MEMORY_STALL", memory_stall},
             {"DATA_PORT_STALL", data_port_stall},
             {"PIPELINE_FLUSH", pipeline_flush},
             {"BRANCH_RESOLVED", normal_step && !reset && !older_memory_fault && !ex_preterminal && next_ex_mem.valid && next_ex_mem.control.branch!=rv32i::RV32IBranchType::None},
             {"BRANCH_MISS", normal_step && !reset && !older_memory_fault && ex_redirect && next_ex_mem.control.branch!=rv32i::RV32IBranchType::None},
             {"PREDICTION_RECOVERY", normal_step && !reset && !older_memory_fault && ex_redirect},
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

void RV32IFiveStageCoreBehavioral::evaluate(
    size_t current_time,
    Simulator& simulator) {
    const bool enabled=high(getInputValue("ENABLE")) && !halted_ && !trapped_;
    const auto old_fetch_pc=fetch_pc_, old_data_address=ex_mem_.alu_result;
    const unsigned old_size=(loadReadActive()||storeWriteActive())?memorySizeEncoding(ex_mem_.control.mem_size):0;
    const bool issue_instruction=enabled;
    const bool issue_data=loadReadActive() || storeWriteActive();
    const bool write_data=storeWriteActive();
    const auto old_store=ex_mem_.store_data;
    const bool take_instruction=high(getOutputPin("IMEM_RSP_READY")->getValue());
    const bool take_data=high(getOutputPin("DMEM_RSP_READY")->getValue());
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
    const bool edge=previous_clk_==LogicValue::LOW && clk==LogicValue::HIGH;
    instruction_request_.tick(*this,"IMEM_",rst==LogicValue::HIGH,edge,
        issue_instruction,take_instruction,old_fetch_pc,0,old_fetch_pc,0,15);
    data_request_.tick(*this,"DMEM_",rst==LogicValue::HIGH,edge,
        issue_data,take_data,old_data_address,write_data?1:0,
        rv32i::memoryWordAddress(old_data_address,old_size),old_store<<((old_data_address&3)*8),
        rv32i::memoryWriteMask(old_data_address,old_size));
    previous_clk_ = clk;
    driveOutputs(current_time, simulator);
}

rv32i::RV32IArchitecturalState
RV32IFiveStageCoreBehavioral::snapshotPipelineState(uint32_t observed_pc) const {
    rv32i::RV32IState state;
    state.pc = observed_pc;
    state.x = registers_;
    state.halted = halted_;
    state.trapped = trapped_;
    state.trap_cause = trap_cause_;
    return rv32i::RV32IArchitecturalState::fromKnown(state);
}

RV32IRetirementObservation RV32IFiveStageCoreBehavioral::pendingRetirement() const {
    return {mem_wb_.pc, mem_wb_.instruction, rv32i::observeMemoryRetirement(
        mem_wb_.control, mem_wb_.alu_result, mem_wb_.store_data, mem_wb_.memory_data,
        mem_wb_.data_misaligned || mem_wb_.data_fault), mem_wb_.next_pc,
        retirementTrap(mem_wb_) != rv32i::RV32IExecutionTrapCause::None || mem_wb_.control.halt};
}
