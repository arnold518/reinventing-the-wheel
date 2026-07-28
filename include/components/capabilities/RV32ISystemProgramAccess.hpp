#pragma once

#include "components/capabilities/RV32IStateView.hpp"
#include "rv32i/RV32IInstructionTrace.hpp"
#include "rv32i/RV32IProgram.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

/**
 * Fidelity-independent setup and observation used by RV32I program scenarios.
 *
 * The visualizer can rebuild a program with either system fidelity without
 * knowing which concrete implementation the profile selected.
 */
class RV32ISystemProgramAccess : public RV32IStateView {
public:
    virtual ~RV32ISystemProgramAccess() = default;

    virtual void clearInstructionMemory() = 0;
    virtual void clearDataMemory() = 0;
    virtual void loadProgram(
        const rv32i::RV32IProgram& program,
        uint32_t base_address = 0) = 0;
    virtual void loadDataBytes(
        uint32_t base_address,
        const std::vector<uint8_t>& data) = 0;

    virtual rv32i::RV32IMemoryTrace
    lastCommittedDataMemoryAccess() const = 0;
    virtual std::map<uint32_t, uint8_t>
    dataMemoryWritesInTimeRange(
        size_t start_time,
        size_t end_time) const = 0;
};
