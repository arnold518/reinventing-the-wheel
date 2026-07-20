#pragma once

#include "components/IOComponent.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>
#include <memory>

class ALU32;
class RegisterFile32x32;
class RV32IControlFlowUnit;
class RV32IDecodeControlUnit;
class RV32IExecutionControlStatusUnit;

class RV32ISingleCycleCore : public IOComponent {
public:
    explicit RV32ISingleCycleCore(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleCore";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;

    rv32i::RV32IState snapshotState(uint64_t instruction_count = 0) const;

    std::shared_ptr<RV32IControlFlowUnit> controlFlow() const { return control_flow_; }
    std::shared_ptr<RV32IDecodeControlUnit> decodeControl() const { return decode_control_; }
    std::shared_ptr<RegisterFile32x32> registerFile() const { return register_file_; }
    std::shared_ptr<ALU32> alu() const { return alu_; }
    std::shared_ptr<RV32IExecutionControlStatusUnit> executionStatus() const { return execution_status_; }

private:
    std::shared_ptr<RV32IControlFlowUnit> control_flow_{};
    std::shared_ptr<RV32IDecodeControlUnit> decode_control_{};
    std::shared_ptr<RegisterFile32x32> register_file_{};
    std::shared_ptr<ALU32> alu_{};
    std::shared_ptr<RV32IExecutionControlStatusUnit> execution_status_{};
};
