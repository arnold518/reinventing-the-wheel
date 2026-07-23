#pragma once

#include "components/selection/BuildProfile.hpp"
#include <memory>
#include <string>
#include <vector>

namespace circuit {

class ComponentCatalog;

struct ProfileGeneratorDescriptor {
    std::string id;
    ParameterMap arguments;

    std::string serialize() const;
};

class ProfileGenerator {
public:
    virtual ~ProfileGenerator() = default;
    virtual BuildProfile generate(const ComponentCatalog& catalog,
                                  const ComponentBuildRequest& root) const = 0;
    virtual ProfileGeneratorDescriptor descriptor() const = 0;
};

class RuleBasedProfileGenerator final : public ProfileGenerator {
public:
    RuleBasedProfileGenerator(std::string profile_name,
                              ProfileGeneratorDescriptor descriptor,
                              std::vector<ProfileRule> rules,
                              UnavailableFidelityPolicy unavailable_policy,
                              bool allow_reference = false);

    BuildProfile generate(const ComponentCatalog& catalog,
                          const ComponentBuildRequest& root) const override;
    ProfileGeneratorDescriptor descriptor() const override;

private:
    std::string profile_name_;
    ProfileGeneratorDescriptor descriptor_;
    std::vector<ProfileRule> rules_;
    UnavailableFidelityPolicy unavailable_policy_;
    bool allow_reference_ = false;
};

class OverrideProfileGenerator final : public ProfileGenerator {
public:
    OverrideProfileGenerator(std::unique_ptr<ProfileGenerator> base,
                             std::vector<ProfileRule> overrides,
                             std::string profile_name = {});

    BuildProfile generate(const ComponentCatalog& catalog,
                          const ComponentBuildRequest& root) const override;
    ProfileGeneratorDescriptor descriptor() const override;

private:
    std::unique_ptr<ProfileGenerator> base_;
    std::vector<ProfileRule> overrides_;
    std::string profile_name_;
};

std::unique_ptr<ProfileGenerator> strictAllStructural();
std::unique_ptr<ProfileGenerator> maximallyStructural();
std::unique_ptr<ProfileGenerator> strictAllBehavioral();
std::unique_ptr<ProfileGenerator> structuralThroughDepth(
    size_t maximum_structural_depth,
    UnavailableFidelityPolicy unavailable_policy = UnavailableFidelityPolicy::Error);
std::unique_ptr<ProfileGenerator> presetProfile(const std::string& name);
std::unique_ptr<ProfileGenerator> withOverrides(
    std::unique_ptr<ProfileGenerator> base,
    std::vector<ProfileRule> overrides,
    std::string profile_name = {});

} // namespace circuit
