#pragma once

#include <cstddef>

// The timing of the gate-level DFlipFlop, in ticks. It is a loop of latches,
// so these are found by trying every case (tests/cpp/DFlipFlopTimingTest.cpp),
// not by following paths. Reset is held low; data and clock are 0 or 1.
//
// Setup and the low phase are the smallest values for which the output is
// also no later than with quiet data. Data arriving 2 to 5 ticks before the
// edge is still captured, but the output then appears as late as tick 8.
namespace circuit::dflipflop_timing {

inline constexpr size_t SETUP = 6;                   // data quiet this long before the rising edge
inline constexpr size_t HOLD = 1;                    // and this long after it
inline constexpr size_t CLOCK_TO_OUTPUT_MINIMUM = 3; // first change of Q or Q_BAR after the edge
inline constexpr size_t CLOCK_TO_OUTPUT_MAXIMUM = 4; // last change
inline constexpr size_t MINIMUM_HIGH = 2;            // shortest clock phases that still work
inline constexpr size_t MINIMUM_LOW = 6;

} // namespace circuit::dflipflop_timing
