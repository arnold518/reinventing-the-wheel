#pragma once
#include "analysis/SafeClockTiming.hpp"
#include "basic/LogicValue.hpp"
#include "basic/PinBase.hpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include <map>

namespace circuit::analysis::detail {
struct MemoryTimingCase {
    bool full = false;
    std::map<std::shared_ptr<PinBase>, LogicValue> registers;
};
struct MemoryTimingGroup {
    std::shared_ptr<BehavioralMemoryEndpoint> endpoint;
    std::vector<MemoryTimingCase> cases;
    // Quantified inputs whose binary-at-edge premise must be discharged by
    // the caller when binary-only transitions are enumerated.
    std::vector<std::shared_ptr<PinBase>> binary_controls;
    std::string evidence;
};
// A finite abstraction of the *actual* requester control netlist. Data, tag
// comparison and CPU decisions are nondeterministic (including X/Z); no ISA or
// workload assumptions. Binary controls are enumerated only when explicitly
// requested; that result is conditional on the caller proving binary startup
// and capture/control preservation. Empty means unsupported, not proven unreachable.
std::optional<MemoryTimingGroup> memoryTimingStates(
    const std::shared_ptr<BehavioralMemoryEndpoint>& endpoint,
    const ClockSpecification& clock);
}
