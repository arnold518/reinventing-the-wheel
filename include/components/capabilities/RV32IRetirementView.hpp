#pragma once

#include "rv32i/RV32IInstructionTrace.hpp"

// Read-only observation of the existing MEM/WB packet, not a hardware trace
// register. Sample before the rising edge, and only when COMMIT_VALID is high.
// The caller owns any retirement counter or history it wants to retain.
struct RV32IRetirementObservation {
    uint32_t pc = 0;
    uint32_t instruction = 0;
    rv32i::RV32IMemoryTrace memory{};
    uint32_t next_pc = 0;
    bool terminal = false;
};

class RV32IRetirementView {
public:
    virtual ~RV32IRetirementView() = default;
    virtual RV32IRetirementObservation pendingRetirement() const = 0;
};
