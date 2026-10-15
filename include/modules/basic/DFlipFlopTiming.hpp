#pragma once

#include "components/TimingProfile.hpp"
#include <cstddef>

// The timing of the gate-level DFlipFlop, in ticks, for the active timing
// profile. It is a loop of latches, so these are found by trying every case
// (tests/cpp/DFlipFlopTimingTest.cpp), not by following paths, and stored in
// the profile (components/TimingProfile.hpp). Reset is held low; data and
// clock are 0 or 1.
//
// Setup and the low phase are the smallest values for which the output is
// also no later than with quiet data. Under "unit", data arriving 2 to 5
// ticks before the edge is still captured, but the output then appears as
// late as tick 8.
namespace circuit::dflipflop_timing {

inline const timing::TimingProfile::FlipFlop& contract() { return timing::activeTimingProfile().flip_flop; }
inline size_t setup() { return contract().setup; }                                     // data quiet this long before the rising edge
inline size_t hold() { return contract().hold; }                                       // and this long after it
inline size_t clockToOutputMinimum() { return contract().clock_to_output_minimum; }    // first change of Q or Q_BAR after the edge
inline size_t clockToOutputMaximum() { return contract().clock_to_output_maximum; }    // last change
inline size_t minimumHigh() { return contract().minimum_high; }                        // shortest clock phases that still work
inline size_t minimumLow() { return contract().minimum_low; }

} // namespace circuit::dflipflop_timing
