#include "components/TimingProfile.hpp"
#include <cstdlib>
#include <stdexcept>

namespace circuit::timing {
namespace {

// Every leaf delay of every profile, in one place. Values of a realistic
// profile must cite their source in `sources`; see
// docs/records/2026-10-16-realistic-timing-plan.md.
const std::vector<TimingProfile>& profiles() {
    static const std::vector<TimingProfile> all = [] {
        std::vector<TimingProfile> list;
        TimingProfile unit;
        unit.name = "unit";
        unit.description = "Every gate and memory primitive one tick; the splitter one tick. The timing of all results before realistic profiles.";
        unit.sources = "none: an abstract unit";
        list.push_back(unit);

        // Gate delays in picoseconds: an inverter driving four copies of
        // itself (FO4) at the node, times each gate's fanout-of-four delay in
        // FO4 units. NAND2 1.35 and NOR2 1.55 FO4: Harris, Ho, Wei, Horowitz,
        // "The Fanout-of-4 Inverter Delay Metric" (1997), figs. 2-4. XOR2 4.0
        // FO4: logical effort g = 4, p = 4 (Harris, CMOS VLSI Design lecture
        // catalog) driving four copies, (4*4+4)/5. AND2 and OR2: NAND2/NOR2
        // driving one inverter (logical effort h = 1: (4/3+2)/5, (5/3+2)/5)
        // followed by an FO4 inverter. Wiring parts carry no delay. Memory
        // primitives: one FO4 until geometry-dependent values replace it.
        const auto realistic = [](std::string name, std::string description, double fo4, std::string node_source) {
            TimingProfile profile;
            profile.name = std::move(name);
            profile.description = std::move(description);
            profile.tick_picoseconds = 1;
            const auto ps = [fo4](double multiple) { return static_cast<SimulationTime>(multiple * fo4 + 0.5); };
            profile.not_gate = ps(1.0);
            profile.nand_gate = ps(1.35);
            profile.nor_gate = ps(1.55);
            profile.and_gate = ps((4.0 / 3 + 2) / 5 + 1.0);
            profile.or_gate = ps((5.0 / 3 + 2) / 5 + 1.0);
            profile.xor_gate = ps(4.0);
            profile.splitter = 0;
            profile.memory_primitive = ps(1.0);
            // Placeholders until calibration against the cited memory sources.
            profile.memory.sram_settle = ps(3.0);
            profile.memory.sram_precharge = ps(4.0);
            profile.memory.sram_bitline_base = ps(3.0);
            profile.memory.sram_bitline_per_row = ps(0.1);
            profile.memory.sram_wire_per_side = ps(0.5);
            profile.memory.rom_settle = ps(3.0);
            profile.memory.rom_column_base = ps(4.0);
            profile.memory.rom_column_per_row = ps(0.1);
            for (auto* interval : {&profile.memory.dram_wordline, &profile.memory.dram_sense_base, &profile.memory.dram_restore,
                                   &profile.memory.dram_read, &profile.memory.dram_write_on, &profile.memory.dram_write_off,
                                   &profile.memory.dram_write_recovery, &profile.memory.dram_close, &profile.memory.dram_precharge,
                                   &profile.memory.dram_equalize, &profile.memory.nor_read, &profile.memory.nor_pulse_end,
                                   &profile.memory.nor_recovery})
                *interval = ps(4.0);
            profile.memory.dram_sense_per_row = ps(0.1);
            profile.memory.nor_read_per_row = ps(0.1);
            // 64 ms (8,192 rows per refresh period, Micron MT48LC16M16A2 tREF).
            profile.memory.dram_device_retention = profile.memory.dram_endpoint_retention = 64'000'000'000ull;
            profile.sources = node_source + "; gate multiples: Harris et al. 1997 FO4 metric paper and logical effort";
            return profile;
        };
        // External devices from the PDF-checked tables (realistic-timing plan,
        // 2026-10-16): absolute intervals, no per-row growth. NOR flash:
        // Infineon S29GL256S (GL-S), tACC 120 ns, single-word program 150 us
        // and 128 KB sector erase 410 ms typical.
        const auto nor = [](TimingProfile& p) {
            p.memory.nor_read = 120000; p.memory.nor_read_per_row = 0;
            p.memory.nor_program = 150'000'000; p.memory.nor_erase = 410'000'000'000ull;
        };
        auto embedded = realistic("embedded", "65 nm microcontroller-class logic; 1 tick = 1 ps", 30.0,
            "FO4 30 ps at 65 nm: arXiv 2606.21454");
        // SDR SDRAM, Micron MT48LC16M16A2 -6A (Table 19): tRCD = CL = tRP = 18 ns,
        // tRAS 42, tWR 12, tRFC 60, tREF 64 ms for 8,192 rows (tREFI 7.8125 us);
        // two x16 chips on a 32-bit channel: 2 KiB rows of 512 words.
        {
            auto& m = embedded.memory;
            m.dram_wordline = 3000; m.dram_sense_base = 12000; m.dram_sense_per_row = 0; m.dram_restore = 3000;
            m.dram_read = 18000;
            m.dram_write_on = 6000; m.dram_write_off = 3000; m.dram_write_recovery = 3000;
            m.dram_close = 3000; m.dram_precharge = 12000; m.dram_equalize = 3000;
            m.dram_refresh_restore = 27000; m.dram_refresh_extra = 0;
            m.dram_refresh_interval = 7'812'500; m.dram_row_words = 512;
            nor(embedded);
        }
        // Measured by DFlipFlopTimingTest and SRAMCellTimingTest under these delays.
        embedded.flip_flop = {231, 0, 132, 175, 52, 231};
        embedded.sram_cell = {71, 112, 42};
        list.push_back(embedded);
        auto desktop = realistic("desktop", "32 nm logic; 1 tick = 1 ps", 21.0,
            "FO4 21 ps at 32 nm: log-linear between 30 ps at 65 nm and about 10 ps at 7 nm (arXiv 2606.21454)");
        desktop.flip_flop = {160, 0, 91, 120, 36, 160};
        // DDR4, Micron MT40A1G8 -062E (DDR4-3200, Tables 159 and 51): tRCD =
        // tAA = tRP = 13.75 ns, tRAS 32, tWR 15, tRFC1 350 (8 Gb), tREFI
        // 7.8125 us; four x8 chips (1 KB pages) on a 32-bit channel: 4 KiB
        // rows of 1,024 words.
        {
            auto& m = desktop.memory;
            m.dram_wordline = 2500; m.dram_sense_base = 9000; m.dram_sense_per_row = 0; m.dram_restore = 2250;
            m.dram_read = 13750;
            m.dram_write_on = 7500; m.dram_write_off = 3750; m.dram_write_recovery = 3750;
            m.dram_close = 2500; m.dram_precharge = 9000; m.dram_equalize = 2250;
            m.dram_refresh_restore = 20500; m.dram_refresh_extra = 304250;
            m.dram_refresh_interval = 7'812'500; m.dram_row_words = 1024;
            nor(desktop);
        }
        desktop.sram_cell = {49, 77, 29};
        list.push_back(desktop);
        return list;
    }();
    return all;
}

const TimingProfile* active = nullptr;

} // namespace

SimulationTime TimingProfile::delay(Leaf leaf) const {
    switch (leaf) {
    case Leaf::Not: return not_gate;
    case Leaf::And: return and_gate;
    case Leaf::Nand: return nand_gate;
    case Leaf::Or: return or_gate;
    case Leaf::Nor: return nor_gate;
    case Leaf::Xor: return xor_gate;
    case Leaf::Splitter: return splitter;
    case Leaf::MemoryPrimitive: return memory_primitive;
    }
    throw std::invalid_argument("Unknown leaf part");
}

const TimingProfile& timingProfile(std::string_view name) {
    for (const auto& profile : profiles())
        if (profile.name == name) return profile;
    throw std::invalid_argument("Unknown timing profile: " + std::string(name));
}

const TimingProfile& activeTimingProfile() {
    if (!active) {
        const char* chosen = std::getenv("CIRCUITSIM_TIMING_PROFILE");
        active = &timingProfile(chosen && *chosen ? chosen : "unit");
    }
    return *active;
}

void setActiveTimingProfile(std::string_view name) { active = &timingProfile(name); }

std::vector<std::string> timingProfileNames() {
    std::vector<std::string> names;
    for (const auto& profile : profiles()) names.push_back(profile.name);
    return names;
}

} // namespace circuit::timing
