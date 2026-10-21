#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>

// Simulation ticks are independent of host pointer/index width and guest ISA.
using SimulationTime = std::uint64_t;

inline constexpr SimulationTime MAX_SIMULATION_TIME =
    std::numeric_limits<SimulationTime>::max();
inline constexpr SimulationTime MAX_EXACT_JSON_TIME =
    (SimulationTime{1} << 53) - 1;

constexpr SimulationTime checkedTimeAdd(SimulationTime time, SimulationTime delay) {
    if (delay > MAX_SIMULATION_TIME - time)
        throw std::overflow_error("Simulation time addition overflow");
    return time + delay;
}

constexpr SimulationTime checkedTimeMultiply(SimulationTime ticks, SimulationTime count) {
    if (count && ticks > MAX_SIMULATION_TIME / count)
        throw std::overflow_error("Simulation time multiplication overflow");
    return ticks * count;
}
