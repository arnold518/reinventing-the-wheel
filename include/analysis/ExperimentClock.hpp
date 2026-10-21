#pragma once

#include "simulator/SimulationTime.hpp"
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
    SimulationTime period = 4000;
    explicit ExperimentClock(SimulationTime value=4000):period(value) {
        if(value<4 || value%2 || value>MAX_SIMULATION_TIME/8)
            throw std::invalid_argument("Clock period must be even and >= 4");
    }
    SimulationTime resetRelease() const {return 2*period;}
    SimulationTime origin() const {return 3*period;}
    SimulationTime edge(size_t cycle) const {
        if(cycle>(MAX_SIMULATION_TIME-origin()-period)/period)
            throw std::overflow_error("Clock timestamp overflow");
        return origin()+cycle*period+1;
    }
    SimulationTime falling(size_t cycle) const {return edge(cycle)+period/2;}
    SimulationTime end(size_t cycle) const {return edge(cycle)+period-1;}
};

// The clock the safe-clock analysis certifies for each core with zero-wait
// FF memory (`rv32i_experiment --period auto`), per timing profile, in that
// profile's ticks. Stored so that fixtures need not run the analysis;
// RV32IProfileSweepTest checks every entry against the analysis.
struct CertifiedCorePeriod { std::string_view profile, core; SimulationTime period; };
inline constexpr std::array<CertifiedCorePeriod, 9> CERTIFIED_CORE_PERIODS{{
    {"unit", "single", 166}, {"unit", "baseline", 80}, {"unit", "advanced", 94},
    {"embedded", "single", 7164}, {"embedded", "baseline", 3354}, {"embedded", "advanced", 4104},
    {"desktop", "single", 4986}, {"desktop", "baseline", 2334}, {"desktop", "advanced", 2858},
}};
inline std::optional<SimulationTime> certifiedCorePeriod(std::string_view profile, std::string_view core) {
    for(const auto& entry:CERTIFIED_CORE_PERIODS)
        if(entry.profile==profile && entry.core==core) return entry.period;
    return std::nullopt;
}
}
