#pragma once

#include <cstddef>

namespace rv32i::validation {

inline constexpr size_t CorrectnessProgramCount = 16;
inline constexpr size_t FirstPerformanceProgram =
    CorrectnessProgramCount + 1;
inline constexpr size_t ProgramCount = 22;

} // namespace rv32i::validation
