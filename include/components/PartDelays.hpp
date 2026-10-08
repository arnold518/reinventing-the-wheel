#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace circuit {

struct DelayRange {
    size_t minimum = 0, maximum = 0;
};

// The timing of one evaluated part as seen from outside, in ticks.
//
// The simulator publishes an output `maximum` ticks after the input that
// caused it. The timing analysis reads the same table as the part's contract
// and also uses `minimum`, the earliest the gate-level form can change, for
// hold. A part without a table keeps its single constructor delay.
struct PartDelays {
    struct Arc {
        std::string input, output;
        DelayRange delay;
    };
    // Where the numbers come from, for example the structural implementation
    // they were proven on.
    std::string evidence;
    // Combinational: a change of `input` can change `output`.
    std::vector<Arc> arcs;
    // Optional: when this input ends at a listed binary value, these arcs
    // replace `arcs` (for example one set per operation code).
    std::string control;
    std::map<uint64_t, std::vector<Arc>> arcs_by_control;
    // Clocked parts. Outputs change `launches` after the rising edge; inputs
    // listed in `setup` are sampled at it and must be quiet that long before.
    std::string clock;
    std::map<std::string, DelayRange> launches;
    std::map<std::string, size_t> setup;
    // Longest register-to-register path inside the part, setup included.
    size_t internal_period = 0;
    // Inputs the table assumes are held, for example RST = 0.
    std::map<std::string, uint64_t> fixed_inputs;

    bool empty() const { return arcs.empty() && launches.empty() && setup.empty(); }
};

} // namespace circuit
