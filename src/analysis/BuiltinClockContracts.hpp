#pragma once

#include "analysis/SafeClockTiming.hpp"
#include <optional>

class IOComponent;

namespace circuit::analysis::detail {

// The exact-implementation contracts used by computeSafeClockPeriod, shared so
// that every analysis trusts the same set of primitives.
std::optional<ClockComponentContract> builtinClockContract(const IOComponent& component);

} // namespace circuit::analysis::detail
