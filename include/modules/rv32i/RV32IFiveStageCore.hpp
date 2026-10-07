#pragma once

#include "components/IOComponent.hpp"
#include "components/capabilities/RV32IPipelineStateView.hpp"
#include "components/capabilities/RV32IRetirementView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "rv32i/RV32IPrediction.hpp"
#include <memory>

namespace circuit::families {
extern const ComponentFamily RV32IFiveStageCore;
}

/**
 * Structural five-stage, single-issue, in-order RV32I core.
 *
 * Transaction-memory counterpart of the direct-memory baseline. Per-stage
 * outputs and a read-only MEM/WB view support tests and visualization without
 * adding saved retirement traces or counters to the hardware.
 */
class RV32IFiveStageCore : public IOComponent, public RV32IPipelineStateView, public RV32IRetirementView {
public:
    explicit RV32IFiveStageCore(std::string name, rv32i::PredictionConfig prediction = {});
    static constexpr const char* TypeName = "RV32IFiveStageCore";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;

    rv32i::RV32IArchitecturalState
    snapshotPipelineState(uint32_t observed_pc) const override;

    RV32IRetirementObservation pendingRetirement() const override;

private:
    rv32i::PredictionConfig prediction_;
    std::shared_ptr<IOComponent> retirement_packet_;
    std::shared_ptr<IOComponent> decode_stage_{};
    std::shared_ptr<IOComponent> writeback_stage_{};
    std::shared_ptr<IOComponent> execution_status_{};
};
