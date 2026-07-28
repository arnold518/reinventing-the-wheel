#pragma once

#include "components/IOComponent.hpp"
#include "components/capabilities/RV32IStateView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "rv32i/RV32IArchitecturalState.hpp"
#include <cstdint>
#include <memory>

namespace circuit::families {
extern const ComponentFamily RV32ISingleCycleCore;
}

class RV32ISingleCycleCore : public IOComponent, public RV32IStateView {
public:
    explicit RV32ISingleCycleCore(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleCore";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;

    rv32i::RV32IArchitecturalState
    snapshotArchitecturalState() const override;

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
