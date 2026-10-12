#pragma once

#include "components/BasicComponent.hpp"
#include "RV32IMemoryRequestModel.hpp"
#include "components/capabilities/RV32IPipelineStateView.hpp"
#include "components/capabilities/RV32IRetirementView.hpp"
#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IInstructionTrace.hpp"
#include "rv32i/RV32IPrediction.hpp"
#include <array>
#include <cstdint>

/**
 * Compact cycle model selected by the RV32IFiveStageCore family.
 *
 * This is a fidelity implementation, not the architectural oracle. It models
 * the same IF/ID/EX/MEM/WB valid, forwarding, stall, flush, and retirement
 * rules as the structural core.
 */
class RV32IFiveStageCoreBehavioral
    : public BasicComponent,
      public RV32IPipelineStateView, public RV32IRetirementView {
public:
    explicit RV32IFiveStageCoreBehavioral(std::string name, rv32i::PredictionConfig prediction = {});
    static constexpr const char* TypeName = "RV32IFiveStageCore";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override;
    rv32i::RV32IArchitecturalState
    snapshotPipelineState(uint32_t observed_pc) const override;

    RV32IRetirementObservation pendingRetirement() const override;

private:
    struct FetchDecodeState {
        uint32_t predicted_next_pc = 0;
        bool valid = false;
        uint32_t pc = 0;
        uint32_t instruction = 0;
        bool pc_misaligned = false;
        bool instruction_fault = false;
    };

    struct DecodeExecuteState {
        uint32_t predicted_next_pc = 0;
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
        bool branch_taken = false;
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

    rv32i::PredictionState prediction_;
    rv32i::MemoryRequestState instruction_request_, data_request_;
    std::array<uint32_t, 32> registers_{};
    uint32_t fetch_pc_ = 0;
    bool halted_ = false;
    bool trapped_ = false;
    rv32i::RV32IExecutionTrapCause trap_cause_ =
        rv32i::RV32IExecutionTrapCause::None;
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
    bool state_unknown_ = false;

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
