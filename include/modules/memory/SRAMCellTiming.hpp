#pragma once

#include <cstddef>

// The timing of the gate-level SRAMCell, in ticks. Its bit sits in a latch, a
// loop of two gates, so these are found by trying every case
// (tests/cpp/SRAMCellTimingTest.cpp), not by following paths: both stored
// values, every old and new value of the four inputs in 0, 1, X and Z, and a
// second change arriving while the first is still settling.
namespace circuit::sram_cell_timing {

inline constexpr size_t EARLIEST = 2; // first change of Q or Q_BAR after an input change
inline constexpr size_t LATEST = 3;   // last change
// Input changes closer together than this can leave the latch oscillating.
// The SRAM access control spaces its phases far wider (phase_ticks >= 32).
inline constexpr size_t MINIMUM_SPACING = 2;

} // namespace circuit::sram_cell_timing
