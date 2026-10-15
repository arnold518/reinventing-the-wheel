#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// The delays of the leaf parts: the parts that have no gate-level form of
// their own. Every other part takes its timing from its gates, either by
// being built from them or, in compact form, from the delay table proven on
// them (ProvenPartDelays) for the same profile.
//
// One profile is active for the whole process. Choose it before building a
// circuit: a part reads its delay when it is constructed. The environment
// variable CIRCUITSIM_TIMING_PROFILE selects the starting profile; the
// default is "unit", every gate one tick, the timing all earlier results use.
namespace circuit::timing {

enum class Leaf {
    Not, And, Nand, Or, Nor, Xor,
    // A bus splitter is wiring. It has carried one tick of delay since it was
    // written, and "unit" keeps that; realistic profiles set it to zero.
    Splitter,
    // The symbolic memory primitives that stand for electrical behavior
    // (cell access, bitlines, sense amplifiers, read columns, output buffers,
    // DRAM and flash cells) and the memory control primitives that are still
    // written in C++.
    MemoryPrimitive,
};

struct TimingProfile {
    std::string name;
    std::string description;
    // Picoseconds per tick, or 0 for an abstract tick.
    size_t tick_picoseconds = 0;
    size_t not_gate = 1, and_gate = 1, nand_gate = 1, or_gate = 1, nor_gate = 1, xor_gate = 1;
    size_t splitter = 1;
    size_t memory_primitive = 1;
    // Measured, not chosen: the contracts of the two gate-level loops that
    // path analysis cannot bound, found by trying every case under this
    // profile's gate delays (DFlipFlopTimingTest, SRAMCellTimingTest check
    // them). Values in ticks.
    struct FlipFlop {
        size_t setup = 6, hold = 1, clock_to_output_minimum = 3, clock_to_output_maximum = 4,
               minimum_high = 2, minimum_low = 6;
    } flip_flop;
    struct SRAMCell {
        size_t earliest = 2, latest = 3, minimum_spacing = 2;
    } sram_cell;
    // Analog intervals of the memory devices, as delay lines and as the delays
    // of the electrical stand-in primitives that share them (ticks). A
    // replica path uses the same value as the path it copies.
    struct Memory {
        size_t sram_settle = 32, sram_precharge = 32, sram_bitline_base = 64, sram_bitline_per_row = 0;
        size_t rom_settle = 32, rom_column_base = 96, rom_column_per_row = 0;
        // DRAM command intervals: ACTIVATE (wordline, sense, restore),
        // READ (column), WRITE (drive on, drive off, recovery), PRECHARGE
        // (close, precharge, equalize). REFRESH is ACTIVATE then PRECHARGE.
        size_t dram_wordline = 32, dram_sense_base = 32, dram_sense_per_row = 0, dram_restore = 32;
        size_t dram_read = 32, dram_write_on = 32, dram_write_off = 32, dram_write_recovery = 32;
        size_t dram_close = 32, dram_precharge = 32, dram_equalize = 32;
        // NOR flash: the read column's sensing time (base and per row) and the
        // two steps after a program/erase pulse.
        size_t nor_read = 96, nor_read_per_row = 0, nor_pulse_end = 32, nor_recovery = 32;
        size_t norRead(size_t rows) const { return nor_read + nor_read_per_row * rows; }
        size_t dramSense(size_t rows) const { return dram_sense_base + dram_sense_per_row * rows; }
        size_t sramBitline(size_t rows) const { return sram_bitline_base + sram_bitline_per_row * rows; }
        size_t romColumn(size_t rows) const { return rom_column_base + rom_column_per_row * rows; }
    } memory;
    // Where the numbers come from.
    std::string sources;

    size_t delay(Leaf leaf) const;
};

const TimingProfile& activeTimingProfile();
// Throws std::invalid_argument for an unknown name.
void setActiveTimingProfile(std::string_view name);
const TimingProfile& timingProfile(std::string_view name);
std::vector<std::string> timingProfileNames();

inline size_t leafDelay(Leaf leaf) { return activeTimingProfile().delay(leaf); }
inline bool unitTiming() { return activeTimingProfile().name == "unit"; }

} // namespace circuit::timing
