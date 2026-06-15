#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace rv32i {

enum class RV32IExecutionTrapCause {
    None,
    IllegalInstruction,
    EnvironmentCall,
    InstructionAddressMisaligned,
    InstructionAccessFault,
    LoadAddressMisaligned,
    LoadAccessFault,
    StoreAddressMisaligned,
    StoreAccessFault,
    InstructionLimit,
};

struct RV32IState {
    uint32_t pc = 0;
    std::array<uint32_t, 32> x{};
    bool halted = false;
    bool trapped = false;
    RV32IExecutionTrapCause trap_cause = RV32IExecutionTrapCause::None;
    uint64_t instruction_count = 0;

    uint32_t readRegister(uint8_t index) const;
    void writeRegister(uint8_t index, uint32_t value);
    void forceX0();
};

std::string_view toString(RV32IExecutionTrapCause cause);

}
