#pragma once

#include "tests/RV32IInstructionLockstepTests.hpp"
#include "rv32i/RV32IValidationConfig.hpp"
#include <cstddef>
#include <map>
#include <string>

inline constexpr size_t RV32ICorrectnessProgramCaseCount =
    rv32i::validation::CorrectnessProgramCount;
inline constexpr size_t RV32IPerformanceProgramCaseFirst =
    rv32i::validation::FirstPerformanceProgram;
inline constexpr size_t RV32IProgramCaseCount =
    rv32i::validation::ProgramCount;

RV32ISystemProgramCase rv32iProgramCase(size_t program_number);
std::string rv32iProgramScenarioName(size_t program_number);
bool rv32iProgramCaseIsPerformance(size_t program_number);

void verifyRV32IProgramExpectedResult(
    const RV32ISystemProgramCase& test_case,
    const rv32i::RV32IState& state,
    const std::map<uint32_t, uint8_t>& bus_writes,
    const std::string& test_name);
