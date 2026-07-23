#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstdint>
#include <memory>

namespace circuit::families {
extern const ComponentFamily RV32ISingleCycleCore;
}

class RV32ISingleCycleCore : public IOComponent {
public:
    explicit RV32ISingleCycleCore(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleCore";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;

    rv32i::RV32IState snapshotState(uint64_t instruction_count = 0) const;

    std::shared_ptr<IOComponent> controlFlow() const { return control_flow_; }
    std::shared_ptr<IOComponent> decodeControl() const { return decode_control_; }
    std::shared_ptr<IOComponent> registerFile() const { return register_file_; }
    std::shared_ptr<IOComponent> alu() const { return alu_; }
    std::shared_ptr<IOComponent> executionStatus() const { return execution_status_; }

private:
    std::shared_ptr<IOComponent> control_flow_{};
    std::shared_ptr<IOComponent> decode_control_{};
    std::shared_ptr<IOComponent> register_file_{};
    std::shared_ptr<IOComponent> alu_{};
    std::shared_ptr<IOComponent> execution_status_{};
};
