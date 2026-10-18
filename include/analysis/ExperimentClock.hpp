#pragma once
#include <cstddef>
#include <limits>
#include <array>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace circuit::analysis {
// Absolute timestamps: never derive a clock edge from the last simulator event.
// Reset gets two periods, then one complete period for initial combinational settle.
struct ExperimentClock {
    size_t period = 4000;
    explicit ExperimentClock(size_t value=4000):period(value) {
        if(value<4 || value%2 || value>std::numeric_limits<size_t>::max()/8)
            throw std::invalid_argument("Clock period must be even and >= 4");
    }
    size_t resetRelease() const {return 2*period;}
    size_t origin() const {return 3*period;}
    size_t edge(size_t cycle) const {
        if(cycle>(std::numeric_limits<size_t>::max()-origin()-period)/period)
            throw std::overflow_error("Clock timestamp overflow");
        return origin()+cycle*period+1;
    }
    size_t falling(size_t cycle) const {return edge(cycle)+period/2;}
    size_t end(size_t cycle) const {return edge(cycle)+period-1;}
};

// The clock the safe-clock analysis certifies for each core with zero-wait
// FF memory (`rv32i_experiment --period auto`), per timing profile, in that
// profile's ticks. Stored so that fixtures need not run the analysis;
// RV32IProfileSweepTest checks every entry against the analysis.
struct CertifiedCorePeriod { std::string_view profile, core; size_t period; };
inline constexpr std::array<CertifiedCorePeriod, 9> CERTIFIED_CORE_PERIODS{{
    {"unit", "single", 166}, {"unit", "baseline", 80}, {"unit", "advanced", 94},
    {"embedded", "single", 7164}, {"embedded", "baseline", 3354}, {"embedded", "advanced", 4104},
    {"desktop", "single", 4986}, {"desktop", "baseline", 2334}, {"desktop", "advanced", 2858},
}};
inline std::optional<size_t> certifiedCorePeriod(std::string_view profile, std::string_view core) {
    for(const auto& entry:CERTIFIED_CORE_PERIODS)
        if(entry.profile==profile && entry.core==core) return entry.period;
    return std::nullopt;
}
}
