#pragma once

#include "modules/memory/Memory64Kx32.hpp"
#include "tests/RV32IInstructionLockstepTests.hpp"
#include <cstddef>
#include <string>

RV32ISystemProgramCase rv32iProgramCase(size_t program_number);

void verifyRV32IProgramExpectedResult(
    const RV32ISystemProgramCase& test_case,
    const rv32i::RV32IState& state,
    const Memory64Kx32& data_memory,
    size_t current_time,
    const std::string& test_name);
