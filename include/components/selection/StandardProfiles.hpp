#pragma once

#include "components/selection/BuildProfile.hpp"

namespace circuit {

/**
 * Convenience factories for ordinary BuildProfile values.
 *
 * A generated profile is an ordinary BuildProfile. There is no separate
 * polymorphic generator object or second selection mechanism.
 */
BuildProfile strictAllStructural();
BuildProfile maximallyStructural();
BuildProfile strictAllBehavioral();
BuildProfile structuralThroughDepth(
    size_t maximum_structural_depth,
    UnavailableFidelityPolicy unavailable_policy =
        UnavailableFidelityPolicy::Error);

} // namespace circuit
