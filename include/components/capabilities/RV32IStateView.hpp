#pragma once

#include "rv32i/RV32IArchitecturalState.hpp"

/**
 * Fidelity-independent observation of RV32I architectural state.
 *
 * Code that hosts or tests an RV32I component depends on this capability,
 * not on the concrete class selected by a build profile.
 */
class RV32IStateView {
public:
    virtual ~RV32IStateView() = default;
    virtual rv32i::RV32IArchitecturalState
    snapshotArchitecturalState() const = 0;
};
