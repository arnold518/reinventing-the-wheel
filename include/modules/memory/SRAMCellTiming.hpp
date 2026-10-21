#pragma once

#include "components/TimingProfile.hpp"
#include <cstddef>

// The timing of the gate-level SRAMCell, in ticks, for the active timing
// profile. Its bit sits in a latch, a loop of two gates, so these are found
// by trying every case (tests/cpp/SRAMCellTimingTest.cpp), not by following
// paths: both stored values, every old and new value of the four inputs in 0,
// 1, X and Z, and a second change arriving while the first is still settling.
// The values are stored in the profile (components/TimingProfile.hpp).
namespace circuit::sram_cell_timing {

inline const timing::TimingProfile::SRAMCell& contract() { return timing::activeTimingProfile().sram_cell; }
inline SimulationTime earliest() { return contract().earliest; }              // first change of Q or Q_BAR after an input change
inline SimulationTime latest() { return contract().latest; }                  // last change
// Input changes closer together than this can leave the latch oscillating.
// The SRAM access control spaces its phases far wider.
inline SimulationTime minimumSpacing() { return contract().minimum_spacing; }

} // namespace circuit::sram_cell_timing
