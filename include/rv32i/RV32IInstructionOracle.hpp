#pragma once

#include "rv32i/RV32IFunctionalMemory.hpp"
#include "rv32i/RV32IState.hpp"
#include "rv32i/RV32IInstructionTrace.hpp"
#include <cstddef>
#include <vector>

class BehavioralMemory64Kx32;

namespace rv32i {

struct RV32IInstructionRunResult {
    RV32IState final_state{};
    std::vector<RV32IInstructionTrace> trace;
    bool instruction_limit_reached = false;
};

class RV32IInstructionOracle {
public:
    static RV32IInstructionTrace step(RV32IState& state, RV32IFunctionalMemory& memory);
    static RV32IInstructionTrace step(RV32IState& state,
                                      RV32IFunctionalMemory& instruction_memory,
                                      RV32IFunctionalMemory& data_memory);
    static RV32IInstructionTrace stepWithoutDataMemoryWrite(RV32IState& state,
                                                            BehavioralMemory64Kx32& instruction_memory,
                                                            BehavioralMemory64Kx32& data_memory);
    static RV32IInstructionRunResult run(RV32IState& state, RV32IFunctionalMemory& memory, size_t max_instructions);
};

}
