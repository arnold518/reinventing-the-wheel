#pragma once

#include "components/selection/SelectionTypes.hpp"
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace circuit {

class ComponentFamily;

struct ProfileSelector {
    std::optional<std::string> exact_path;
    std::optional<std::string> subtree_path;
    std::optional<std::string> contract_id;
    std::optional<size_t> minimum_depth;
    std::optional<size_t> maximum_depth;
    ParameterMap parameters;

    bool matches(const std::string& path, size_t depth,
                 const std::string& requested_contract,
                 const ParameterMap& requested_parameters) const;
    std::string serialize() const;

    static ProfileSelector any();
    static ProfileSelector exactPath(std::string path);
    static ProfileSelector subtree(std::string path);
    static ProfileSelector contract(std::string id);
    static ProfileSelector contract(const ComponentFamily& family);
    static ProfileSelector depths(size_t minimum,
                                  std::optional<size_t> maximum = std::nullopt);
};

struct ProfileRule {
    ProfileSelector selector;
    Fidelity fidelity = Fidelity::Structural;
    std::string reason;

    std::string serialize() const;
};

struct ProfileDecision {
    std::optional<Fidelity> fidelity;
    std::string reason;
    bool matched_rule = false;
};

class BuildProfile {
public:
    BuildProfile(std::string name, std::vector<ProfileRule> rules,
                 UnavailableFidelityPolicy unavailable_policy);

    ProfileDecision decide(const std::string& path, size_t depth,
                           const std::string& contract_id,
                           const ParameterMap& parameters) const;

    const std::string& name() const;
    const std::vector<ProfileRule>& rules() const;
    UnavailableFidelityPolicy unavailablePolicy() const;
    std::string serialize() const;
    std::string fingerprint() const;

private:
    std::string name_;
    std::vector<ProfileRule> rules_;
    UnavailableFidelityPolicy unavailable_policy_;
};

class BuildProfileBuilder {
public:
    explicit BuildProfileBuilder(std::string name);

    BuildProfileBuilder& addRule(ProfileRule rule);
    BuildProfileBuilder& unavailablePolicy(UnavailableFidelityPolicy policy);
    BuildProfile build() const;

private:
    std::string name_;
    std::vector<ProfileRule> rules_;
    UnavailableFidelityPolicy unavailable_policy_ = UnavailableFidelityPolicy::Error;
};

ProfileRule preferFidelity(Fidelity fidelity,
                           ProfileSelector selector = ProfileSelector::any(),
                           std::string reason = {});
BuildProfile withProfileOverrides(
    BuildProfile base,
    std::vector<ProfileRule> overrides,
    std::string profile_name = {});
BuildProfile withExactFidelity(
    BuildProfile base,
    std::string path,
    Fidelity fidelity,
    std::string profile_name = {});
BuildProfile canonicalDefaultProfile();

} // namespace circuit
