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

    std::shared_ptr<IOComponent> fetchStage() const { return fetch_; }
    std::shared_ptr<IOComponent> decodeStage() const { return decode_; }
    std::shared_ptr<IOComponent> executeStage() const { return execute_; }
    std::shared_ptr<IOComponent> memoryStage() const { return memory_; }
    std::shared_ptr<IOComponent> writebackStage() const { return writeback_; }
    std::shared_ptr<IOComponent> nextPCStage() const { return next_pc_; }
    std::shared_ptr<IOComponent> executionStatus() const { return execution_status_; }

private:
    std::shared_ptr<IOComponent> fetch_{},decode_{},execute_{},memory_{},writeback_{},next_pc_{};
    std::shared_ptr<IOComponent> execution_status_{};
};
