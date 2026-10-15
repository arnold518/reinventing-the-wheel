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
