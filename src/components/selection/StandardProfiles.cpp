#include "components/selection/StandardProfiles.hpp"

namespace circuit {

BuildProfile strictAllStructural() {
    return BuildProfileBuilder("strict-all-structural")
        .addRule(preferFidelity(
            Fidelity::Structural,
            ProfileSelector::any(),
            "strict all structural"))
        .unavailablePolicy(UnavailableFidelityPolicy::Error)
        .build();
}

BuildProfile maximallyStructural() {
    return BuildProfileBuilder("maximally-structural")
        .addRule(preferFidelity(
            Fidelity::Structural,
            ProfileSelector::any(),
            "prefer structural"))
        .unavailablePolicy(
            UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException)
        .build();
}

BuildProfile strictAllBehavioral() {
    return BuildProfileBuilder("strict-all-behavioral")
        .addRule(preferFidelity(
            Fidelity::Behavioral,
            ProfileSelector::any(),
            "strict all behavioral"))
        .unavailablePolicy(UnavailableFidelityPolicy::Error)
        .build();
}

BuildProfile structuralThroughDepth(
    size_t maximum_structural_depth,
    UnavailableFidelityPolicy unavailable_policy) {
    auto builder = BuildProfileBuilder(
        "structural-through-depth-"
        + std::to_string(maximum_structural_depth));
    builder
        .addRule(preferFidelity(
            Fidelity::Structural,
            ProfileSelector::depths(0, maximum_structural_depth),
            "structural through configured depth"));
    if (maximum_structural_depth != static_cast<size_t>(-1)) {
        builder.addRule(preferFidelity(
            Fidelity::Behavioral,
            ProfileSelector::depths(maximum_structural_depth + 1),
            "behavioral below configured structural depth"));
    }
    builder.unavailablePolicy(unavailable_policy);
    return builder.build();
}

} // namespace circuit
