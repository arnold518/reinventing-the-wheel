#pragma once

#include "components/PartDelays.hpp"
#include <memory>
#include <string>

class Component;

namespace circuit::analysis {

// The delay twin of a gate-level part, or null when it cannot have one: the
// part has state, a loop, a clock, or a leaf that is not a plain gate.
// `why`, when given, receives the reason for a null result.
std::shared_ptr<const circuit::DelayTwin> buildDelayTwin(const std::shared_ptr<Component>& gates, std::string* why = nullptr);

} // namespace circuit::analysis
