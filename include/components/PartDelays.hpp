#pragma once

#include <cstddef>
#include "basic/LogicValue.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace circuit {

struct DelayRange {
    size_t minimum = 0, maximum = 0;
};

// The gates a compact part stands for, kept only to time it. Given the value
// every input bit has now and the tick it last changed, one pass over the
// gates tells when each output has its final value. A gate with an input at
// its controlling value (0 into AND, 1 into OR) settles with that input and
// does not wait for the other; this is what makes the answer follow the data,
// for example how far a carry really travels. It holds for any earlier
// history of the inputs, so changes that overlap are covered as well.
//
// A part with flip-flops is covered too: the twin keeps the bit each one
// holds, takes the value at its data input on a rising clock edge, and lets
// its output change one clock-to-output delay later.
struct DelayTwin {
    virtual ~DelayTwin() = default;
    // Pin names and widths, in the order `settle` takes and gives them.
    std::vector<std::pair<std::string, size_t>> inputs, outputs;
    // What one part remembers between changes: the bits its flip-flops hold.
    // Null for a part without state.
    struct State { virtual ~State() = default; };
    virtual std::unique_ptr<State> start() const { return nullptr; }
    // changed[i][bit]: tick of that bit's last change, -1 if it never changed.
    // settled[o]: tick by which output o is final, -1 if nothing can move it.
    // value[o][bit]: what the gates end on there: 0, 1, or 2 for unknown.
    // False: no answer for this change (a reset in progress, an unknown
    // clock); the table applies.
    virtual bool settle(State* state, long long now, const std::vector<std::vector<LogicValue>>& values,
                        const std::vector<std::vector<long long>>& changed, std::vector<long long>& settled,
                        std::vector<std::vector<uint8_t>>& value) const = 0;
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
    // Outputs that pass the clock on unchanged to parts further on.
    std::vector<std::string> clock_outputs;
    std::map<std::string, DelayRange> launches;
    std::map<std::string, size_t> setup;
    // The part also changes outputs at times of its own, between clock edges.
    // `launches` then count from whatever caused the change, and no clock
    // period can be certified around the part.
    bool self_timed = false;
    // Longest register-to-register path inside the part, setup included.
    size_t internal_period = 0;
    // Inputs the table assumes are held, for example RST = 0.
    std::map<std::string, uint64_t> fixed_inputs;

    // Optional: times the part from its gates and the actual values. Never
    // later than the numbers above allow.
    std::shared_ptr<const DelayTwin> twin;

    bool empty() const { return arcs.empty() && launches.empty() && setup.empty(); }
};

} // namespace circuit
