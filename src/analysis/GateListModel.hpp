#pragma once

#include "basic/LogicValue.hpp"
#include <memory>
#include <string>
#include <utility>
#include <vector>

class Component;
class PinBase;

namespace circuit::analysis {

struct GateListProgram;

// A gate-level part run as one compact part: the same gates, evaluated with
// their four-state values, without events between them.
//
// What it runs is the part's own timing graph (TimingGraph): one node per pin
// bit, gates as they are built, compact state bits and registers with their
// own rules (a level reset that clears at once, a rising-edge capture), and
// primitives that describe themselves (Primitive::settled). Only the bits a
// change reaches are recomputed, in the graph's order. A rising clock edge
// takes the values that stood before the inputs given with it, as a
// flip-flop does when its data meet the setup time; the compact part's delay
// table checks that the inputs did.
//
// The rules stay with the gates: a change to the gate-level part changes
// what this runs. Timing is not computed here; the compact part takes it
// from the delay table proven on the same gates.
class GateListModel {
public:
    // Null when the part holds something this cannot run (a loop, a memory
    // endpoint, another kind of state, several clocks); `why` says what.
    static std::shared_ptr<const GateListProgram> compile(const std::shared_ptr<Component>& gates,
        std::string* why = nullptr,
        const std::vector<std::shared_ptr<PinBase>>& observations = {});
    explicit GateListModel(std::shared_ptr<const GateListProgram> program);

    // Root pins, in the order step() takes and output() gives them.
    const std::vector<std::pair<std::string, size_t>>& inputs() const;
    const std::vector<std::pair<std::string, size_t>>& outputs() const;
    // True for an output that passes the clock on unchanged.
    bool passesClock(size_t output) const;

    // The values now on every input pin.
    void step(const std::vector<std::vector<LogicValue>>& inputs);
    std::vector<LogicValue> output(size_t pin) const;
    // Read-only circuit signals selected during compilation. Only node indexes
    // are retained; observing adds no state, pins, or simulated work.
    std::vector<LogicValue> observation(size_t pin) const;
    size_t nodeCount() const;

private:
    void propagate();
    void mark(int node);
    void set(int node, LogicValue value);
    LogicValue compute(int node);

    std::shared_ptr<const GateListProgram> program_;
    std::vector<LogicValue> value_;
    std::vector<int> heap_;
    std::vector<uint8_t> queued_;
    std::vector<unsigned> leaf_pass_;
    std::vector<std::vector<std::pair<std::string, std::vector<LogicValue>>>> leaf_out_;
    unsigned pass_ = 1;
    LogicValue previous_clock_ = LogicValue::UNKNOWN;
};

} // namespace circuit::analysis
