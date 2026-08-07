#pragma once

#include "components/BasicComponent.hpp"
#include "components/capabilities/RV32IStateView.hpp"
#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IInstructionTrace.hpp"
#include <array>
#include <cstdint>

/**
 * Compact cycle model selected by the RV32IFiveStageCore family.
 *
 * This is a fidelity implementation, not the architectural oracle. It models
 * the same IF/ID/EX/MEM/WB valid, forwarding, stall, flush, and retirement
 * rules as the structural core.
 */
class RV32IFiveStageCoreDirect
    : public BasicComponent,
      public RV32IStateView {
public:
    explicit RV32IFiveStageCoreDirect(std::string name);
    static constexpr const char* TypeName = "RV32IFiveStageCore";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override;
    rv32i::RV32IArchitecturalState
    snapshotArchitecturalState() const override;

private:
    struct FetchDecodeState {
        bool valid = false;
        uint32_t pc = 0;
        uint32_t instruction = 0;
        bool pc_misaligned = false;
        bool instruction_fault = false;
    };

    struct DecodeExecuteState {
        bool valid = false;
        uint32_t pc = 0;
        uint32_t instruction = 0;
        uint32_t rs1_value = 0;
        uint32_t rs2_value = 0;
        uint32_t immediate = 0;
        uint8_t rs1 = 0;
        uint8_t rs2 = 0;
        uint8_t rd = 0;
        rv32i::RV32IControlSignals control{};
        bool pc_misaligned = false;
        bool instruction_fault = false;
    };

    struct ExecuteMemoryState {
        bool valid = false;
        uint32_t pc = 0;
        uint32_t instruction = 0;
        uint32_t alu_result = 0;
        uint32_t store_data = 0;
        uint32_t pc_plus_4 = 0;
        uint32_t next_pc = 0;
        uint8_t rd = 0;
        rv32i::RV32IControlSignals control{};
        bool pc_misaligned = false;
        bool instruction_fault = false;
        bool target_misaligned = false;
    };

    struct MemoryWritebackState {
        bool valid = false;
        uint32_t pc = 0;
        uint32_t instruction = 0;
        uint32_t alu_result = 0;
        uint32_t memory_data = 0;
        uint32_t store_data = 0;
        uint32_t pc_plus_4 = 0;
        uint32_t next_pc = 0;
        uint8_t rd = 0;
        rv32i::RV32IControlSignals control{};
        bool pc_misaligned = false;
        bool instruction_fault = false;
        bool target_misaligned = false;
        bool data_misaligned = false;
        bool data_fault = false;
    };

    std::array<uint32_t, 32> registers_{};
    uint32_t fetch_pc_ = 0;
    uint32_t committed_pc_ = 0;
    bool halted_ = false;
    bool trapped_ = false;
    rv32i::RV32IExecutionTrapCause trap_cause_ =
        rv32i::RV32IExecutionTrapCause::None;
    uint32_t retired_count_ = 0;
    uint32_t retired_pc_ = 0;
    uint32_t retired_instruction_ = 0;
    rv32i::RV32IMemoryTrace retired_memory_{};
    LogicValue previous_clk_ = LogicValue::UNKNOWN;

    FetchDecodeState if_id_{};
    DecodeExecuteState id_ex_{};
    ExecuteMemoryState ex_mem_{};
    MemoryWritebackState mem_wb_{};

    void resetState();
    void advanceOneCycle();
    void driveOutputs(size_t current_time, Simulator& simulator);

    DecodeExecuteState decode(
        const FetchDecodeState& state,
        const std::array<uint32_t, 32>& register_snapshot) const;
    ExecuteMemoryState execute(
        const DecodeExecuteState& state) const;
    MemoryWritebackState accessMemory(
        const ExecuteMemoryState& state) const;

    bool loadUseHazard() const;
    bool terminalPending() const;
    bool storeWriteActive() const;
    bool loadReadActive() const;
    uint32_t writebackValue(
        const MemoryWritebackState& state) const;
    rv32i::RV32IExecutionTrapCause retirementTrap(
        const MemoryWritebackState& state) const;
};
