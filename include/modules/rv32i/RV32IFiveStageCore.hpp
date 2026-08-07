#pragma once

#include "components/IOComponent.hpp"
#include "components/capabilities/RV32IStateView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <memory>

namespace circuit::families {
extern const ComponentFamily RV32IFiveStageCore;
}

/**
 * Structural five-stage, single-issue, in-order RV32I core.
 *
 * The public contract deliberately matches the single-cycle memory boundary.
 * Extra outputs expose retirement and per-stage state for tests and the
 * visualizer without changing architectural behavior.
 */
class RV32IFiveStageCore : public IOComponent, public RV32IStateView {
public:
    explicit RV32IFiveStageCore(std::string name);
    static constexpr const char* TypeName = "RV32IFiveStageCore";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;

    rv32i::RV32IArchitecturalState
    snapshotArchitecturalState() const override;

private:
    std::shared_ptr<IOComponent> decode_stage_{};
    std::shared_ptr<IOComponent> writeback_stage_{};
};
