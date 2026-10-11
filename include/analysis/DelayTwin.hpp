#pragma once

#include "components/PartDelays.hpp"
#include <memory>

class Component;

namespace circuit::analysis {

// The delay twin of a gate-level part, or null when it cannot have one: the
// part has state, a loop, a clock, or a leaf that is not a plain gate.
std::shared_ptr<const circuit::DelayTwin> buildDelayTwin(const std::shared_ptr<Component>& gates);

} // namespace circuit::analysis
