#pragma once

#include "tests/RV32IInstructionLockstepTests.hpp"
#include <cstddef>
#include <map>
#include <string>

RV32ISystemProgramCase rv32iProgramCase(size_t program_number);
std::string rv32iProgramScenarioName(size_t program_number);

void verifyRV32IProgramExpectedResult(
    const RV32ISystemProgramCase& test_case,
    const rv32i::RV32IState& state,
    const std::map<uint32_t, uint8_t>& bus_writes,
    const std::string& test_name);
