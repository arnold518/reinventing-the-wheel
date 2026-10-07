#pragma once

#include "components/capabilities/RV32IPipelineStateView.hpp"
#include "components/capabilities/RV32IRetirementView.hpp"
#include "components/capabilities/RV32IStateView.hpp"
#include <memory>
#include <stdexcept>

// Host-side observation only. Not a Component, not part of the datapath,
// and never used to steer fetch, hazards, register writes, or memory requests.
class RV32IPipelineObserver : public RV32IStateView {
public:
    explicit RV32IPipelineObserver(std::shared_ptr<RV32IPipelineStateView> view)
        : view_(std::move(view)) {
        if (!view_) throw std::invalid_argument("Missing pipeline state view");
    }
    void reset() { pc_ = 0; }
    void retire(const RV32IRetirementObservation& packet) {
        if (packet.pc != pc_)
            throw std::logic_error("Retiring packet PC does not match observed instruction sequence");
        if (!packet.terminal) pc_ = packet.next_pc;
    }
    uint32_t pc() const { return pc_; }
    rv32i::RV32IArchitecturalState snapshotArchitecturalState() const override {
        return view_->snapshotPipelineState(pc_);
    }
private:
    std::shared_ptr<RV32IPipelineStateView> view_;
    uint32_t pc_ = 0;
};
