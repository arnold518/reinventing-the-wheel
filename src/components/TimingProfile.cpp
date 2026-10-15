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
            const auto ps = [fo4](double multiple) { return static_cast<size_t>(multiple * fo4 + 0.5); };
            profile.not_gate = ps(1.0);
            profile.nand_gate = ps(1.35);
            profile.nor_gate = ps(1.55);
            profile.and_gate = ps((4.0 / 3 + 2) / 5 + 1.0);
            profile.or_gate = ps((5.0 / 3 + 2) / 5 + 1.0);
            profile.xor_gate = ps(4.0);
            profile.splitter = 0;
            profile.memory_primitive = ps(1.0);
            profile.sources = node_source + "; gate multiples: Harris et al. 1997 FO4 metric paper and logical effort";
            return profile;
        };
        auto embedded = realistic("embedded", "65 nm microcontroller-class logic; 1 tick = 1 ps", 30.0,
            "FO4 30 ps at 65 nm: arXiv 2606.21454");
        // Measured by DFlipFlopTimingTest and SRAMCellTimingTest under these delays.
        embedded.flip_flop = {231, 0, 132, 175, 52, 231};
        embedded.sram_cell = {71, 112, 42};
        list.push_back(embedded);
        auto desktop = realistic("desktop", "32 nm logic; 1 tick = 1 ps", 21.0,
            "FO4 21 ps at 32 nm: log-linear between 30 ps at 65 nm and about 10 ps at 7 nm (arXiv 2606.21454)");
        desktop.flip_flop = {160, 0, 91, 120, 36, 160};
        desktop.sram_cell = {49, 77, 29};
        list.push_back(desktop);
        return list;
    }();
    return all;
}

const TimingProfile* active = nullptr;

} // namespace

size_t TimingProfile::delay(Leaf leaf) const {
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
