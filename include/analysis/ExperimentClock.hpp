#pragma once
#include <cstddef>
#include <limits>
#include <stdexcept>

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
    static size_t recommendedPeriod(size_t path) {
        if(path>std::numeric_limits<size_t>::max()/4) throw std::overflow_error("Timing path overflow");
        // Each half-cycle has >= 25% settle margin plus two event-order ticks.
        return 2*(path+path/4+2);
    }
};
}
