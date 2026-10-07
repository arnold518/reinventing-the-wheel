#pragma once

#include "rv32i/RV32IArchitecturalState.hpp"

// A pipeline has several live PCs, not a committed-PC hardware register.
// The caller supplies its retirement observer's PC when composing ISA state.
class RV32IPipelineStateView {
public:
    virtual ~RV32IPipelineStateView() = default;
    virtual rv32i::RV32IArchitecturalState
    snapshotPipelineState(uint32_t observed_pc) const = 0;
};
