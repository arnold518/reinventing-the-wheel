#pragma once

#include "analysis/CircuitTimingAnalyzer.hpp"
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <unordered_set>
#include <vector>

class Component;
class IOComponent;
class PinBase;

// What every timing analysis needs from a built circuit: which parts are
// treated as indivisible, the contract that describes each one, and which pin
// drives which. Kept in one place so the analyses cannot disagree about it.
namespace circuit::analysis::detail {

struct TimingLeaf {
    std::shared_ptr<IOComponent> component;
    ClockComponentContract contract;
    // The contract is the built-in one for this exact implementation, so the
    // part's own logic (gate function, bit routing) may also be relied on.
    bool builtin_rules = false;
};

// Contracts for concrete primitive implementations only: gates, the bit
// adapters, Rewire, constants, and the compact storage parts and FF endpoint.
// Matched by C++ type, never by name or pin shape.
std::optional<ClockComponentContract> builtinClockContract(const IOComponent& component);
bool isBitSplitter(const IOComponent& component);
bool isBitJoiner(const IOComponent& component);

struct TimingLeafVisitor {
    // Every component with pins, in visiting order, before it is classified.
    std::function<void(const std::shared_ptr<IOComponent>&)> component;
    // An evaluated or childless part that has no contract.
    std::function<void(const Component&)> uncovered;
};

// Walk the hierarchy from `root`, stopping at each component that has a
// contract (the caller's first, then the built-in ones).
void collectTimingLeaves(const std::shared_ptr<Component>& root, const TimingContracts& contracts,
                         size_t maximum_depth, std::vector<TimingLeaf>& leaves,
                         const TimingLeafVisitor& visitor);

// Walk upwards through input boundaries and downwards through output
// boundaries to the pin that drives `pin`. Stops at a pin in `sources`;
// otherwise returns the last pin reached, which nothing drives.
std::shared_ptr<PinBase> drivingPin(std::shared_ptr<PinBase> pin,
                                    const std::unordered_set<const PinBase*>& sources);

} // namespace circuit::analysis::detail
