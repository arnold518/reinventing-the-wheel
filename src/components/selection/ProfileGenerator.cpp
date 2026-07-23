#include "components/selection/ProfileGenerator.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include <sstream>
#include <stdexcept>

namespace circuit {

std::string ProfileGeneratorDescriptor::serialize() const {
    return id + "(" + canonicalizeParameters(arguments) + ")";
}

RuleBasedProfileGenerator::RuleBasedProfileGenerator(
    std::string profile_name,
    ProfileGeneratorDescriptor descriptor,
    std::vector<ProfileRule> rules,
    UnavailableFidelityPolicy unavailable_policy,
    bool allow_reference)
    : profile_name_(std::move(profile_name)),
      descriptor_(std::move(descriptor)),
      rules_(std::move(rules)),
      unavailable_policy_(unavailable_policy),
      allow_reference_(allow_reference) {}

BuildProfile RuleBasedProfileGenerator::generate(
    const ComponentCatalog& catalog,
    const ComponentBuildRequest& root) const {
    if (!catalog.hasContract(root.contract_id)) {
        throw std::runtime_error("Cannot generate profile for unknown root contract '"
                                 + root.contract_id + "'");
    }
    return BuildProfile(profile_name_, rules_, unavailable_policy_, allow_reference_,
                        descriptor_.serialize());
}

ProfileGeneratorDescriptor RuleBasedProfileGenerator::descriptor() const {
    return descriptor_;
}

OverrideProfileGenerator::OverrideProfileGenerator(
    std::unique_ptr<ProfileGenerator> base,
    std::vector<ProfileRule> overrides,
    std::string profile_name)
    : base_(std::move(base)),
      overrides_(std::move(overrides)),
      profile_name_(std::move(profile_name)) {
    if (!base_) {
        throw std::invalid_argument("OverrideProfileGenerator requires a base generator");
    }
    for (auto& rule : overrides_) {
        rule.priority += 1'000;
    }
}

BuildProfile OverrideProfileGenerator::generate(
    const ComponentCatalog& catalog,
    const ComponentBuildRequest& root) const {
    auto base = base_->generate(catalog, root);
    auto rules = base.rules();
    rules.insert(rules.end(), overrides_.begin(), overrides_.end());
    const auto descriptor_text = descriptor().serialize();
    return BuildProfile(profile_name_.empty() ? base.name() + "+overrides" : profile_name_,
                        std::move(rules), base.unavailablePolicy(),
                        base.allowReference(), descriptor_text);
}

ProfileGeneratorDescriptor OverrideProfileGenerator::descriptor() const {
    ParameterMap arguments{
        {"base", base_->descriptor().serialize()},
        {"override_count", std::to_string(overrides_.size())},
    };
    for (size_t index = 0; index < overrides_.size(); ++index) {
        arguments["override_" + std::to_string(index)] = overrides_[index].serialize();
    }
    return {"with-overrides", std::move(arguments)};
}

std::unique_ptr<ProfileGenerator> strictAllStructural() {
    return std::make_unique<RuleBasedProfileGenerator>(
        "strict-all-structural",
        ProfileGeneratorDescriptor{"strict-all-structural", {}},
        std::vector<ProfileRule>{preferFidelity(
            Fidelity::Structural, ProfileSelector::any(), "strict all structural")},
        UnavailableFidelityPolicy::Error);
}

std::unique_ptr<ProfileGenerator> maximallyStructural() {
    return std::make_unique<RuleBasedProfileGenerator>(
        "maximally-structural",
        ProfileGeneratorDescriptor{"maximally-structural", {}},
        std::vector<ProfileRule>{preferFidelity(
            Fidelity::Structural, ProfileSelector::any(), "prefer structural")},
        UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException);
}

std::unique_ptr<ProfileGenerator> strictAllBehavioral() {
    return std::make_unique<RuleBasedProfileGenerator>(
        "strict-all-behavioral",
        ProfileGeneratorDescriptor{"strict-all-behavioral", {}},
        std::vector<ProfileRule>{preferFidelity(
            Fidelity::Behavioral, ProfileSelector::any(), "strict all behavioral")},
        UnavailableFidelityPolicy::Error);
}

std::unique_ptr<ProfileGenerator> structuralThroughDepth(
    size_t maximum_structural_depth,
    UnavailableFidelityPolicy unavailable_policy) {
    std::vector<ProfileRule> rules;
    rules.push_back(preferFidelity(
        Fidelity::Structural,
        ProfileSelector::depths(0, maximum_structural_depth),
        "structural through configured depth"));
    if (maximum_structural_depth != static_cast<size_t>(-1)) {
        rules.push_back(preferFidelity(
            Fidelity::Behavioral,
            ProfileSelector::depths(maximum_structural_depth + 1),
            "behavioral below configured structural depth"));
    }
    return std::make_unique<RuleBasedProfileGenerator>(
        "structural-through-depth-" + std::to_string(maximum_structural_depth),
        ProfileGeneratorDescriptor{
            "structural-through-depth",
            {{"maximum_structural_depth", std::to_string(maximum_structural_depth)},
             {"unavailable_fidelity", toString(unavailable_policy)}}},
        std::move(rules), unavailable_policy);
}

std::unique_ptr<ProfileGenerator> presetProfile(const std::string& name) {
    if (name == "education") {
        return maximallyStructural();
    }
    if (name == "balanced") {
        return structuralThroughDepth(
            2, UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException);
    }
    if (name == "fast") {
        return std::make_unique<RuleBasedProfileGenerator>(
            "fast",
            ProfileGeneratorDescriptor{"preset", {{"name", "fast"}}},
            std::vector<ProfileRule>{preferFidelity(
                Fidelity::Behavioral, ProfileSelector::any(), "fast preset")},
            UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException);
    }
    if (name == "reference") {
        return std::make_unique<RuleBasedProfileGenerator>(
            "reference",
            ProfileGeneratorDescriptor{"preset", {{"name", "reference"}}},
            std::vector<ProfileRule>{preferFidelity(
                Fidelity::Behavioral, ProfileSelector::any(), "reference preset")},
            UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException,
            true);
    }
    throw std::invalid_argument("Unknown profile preset '" + name + "'");
}

std::unique_ptr<ProfileGenerator> withOverrides(
    std::unique_ptr<ProfileGenerator> base,
    std::vector<ProfileRule> overrides,
    std::string profile_name) {
    return std::make_unique<OverrideProfileGenerator>(
        std::move(base), std::move(overrides), std::move(profile_name));
}

} // namespace circuit
