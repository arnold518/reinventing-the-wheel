#include "components/selection/BuildProfile.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <algorithm>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace circuit {
namespace {

bool pathIsInSubtree(const std::string& path, const std::string& subtree) {
    return path == subtree
        || (path.size() > subtree.size()
            && path.compare(0, subtree.size(), subtree) == 0
            && path[subtree.size()] == '.');
}

std::string optionalString(const std::optional<std::string>& value) {
    return value ? *value : "*";
}

std::string optionalSize(const std::optional<size_t>& value) {
    return value ? std::to_string(*value) : "*";
}

uint64_t fnv1a64(const std::string& text) {
    uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : text) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    return hash;
}

} // namespace

bool ProfileSelector::matches(const std::string& path, size_t depth,
                              const std::string& requested_contract,
                              const ParameterMap& requested_parameters) const {
    if (exact_path && path != *exact_path) return false;
    if (subtree_path && !pathIsInSubtree(path, *subtree_path)) return false;
    if (contract_id && requested_contract != *contract_id) return false;
    if (minimum_depth && depth < *minimum_depth) return false;
    if (maximum_depth && depth > *maximum_depth) return false;
    for (const auto& [key, value] : parameters) {
        auto found = requested_parameters.find(key);
        if (found == requested_parameters.end() || found->second != value) {
            return false;
        }
    }
    return true;
}

std::string ProfileSelector::serialize() const {
    std::ostringstream out;
    out << "exact=" << optionalString(exact_path)
        << ",subtree=" << optionalString(subtree_path)
        << ",contract=" << optionalString(contract_id)
        << ",depth=" << optionalSize(minimum_depth)
        << ".." << optionalSize(maximum_depth)
        << ",parameters=" << canonicalizeParameters(parameters);
    return out.str();
}

ProfileSelector ProfileSelector::any() {
    return {};
}

ProfileSelector ProfileSelector::exactPath(std::string path) {
    ProfileSelector selector;
    selector.exact_path = std::move(path);
    return selector;
}

ProfileSelector ProfileSelector::subtree(std::string path) {
    ProfileSelector selector;
    selector.subtree_path = std::move(path);
    return selector;
}

ProfileSelector ProfileSelector::contract(std::string id) {
    ProfileSelector selector;
    selector.contract_id = std::move(id);
    return selector;
}

ProfileSelector ProfileSelector::contract(const ComponentFamily& family) {
    return contract(std::string(family.id()));
}

ProfileSelector ProfileSelector::depths(size_t minimum,
                                        std::optional<size_t> maximum) {
    ProfileSelector selector;
    selector.minimum_depth = minimum;
    selector.maximum_depth = maximum;
    return selector;
}

std::string ProfileRule::serialize() const {
    std::ostringstream out;
    out << selector.serialize()
        << "|fidelity=" << toString(fidelity)
        << "|reason=" << reason;
    return out.str();
}

BuildProfile::BuildProfile(std::string name, std::vector<ProfileRule> rules,
                           UnavailableFidelityPolicy unavailable_policy)
    : name_(std::move(name)),
      rules_(std::move(rules)),
      unavailable_policy_(unavailable_policy) {
    if (name_.empty()) {
        throw std::invalid_argument("BuildProfile name must not be empty");
    }
}

ProfileDecision BuildProfile::decide(const std::string& path, size_t depth,
                                     const std::string& contract_id,
    const ParameterMap& parameters) const {
    const ProfileRule* winner = nullptr;

    for (const auto& rule : rules_) {
        if (!rule.selector.matches(path, depth, contract_id, parameters)) {
            continue;
        }
        // Profiles are ordered policies. Later matching rules are explicit
        // overrides of earlier defaults, so there is no hidden specificity or
        // priority contest.
        winner = &rule;
    }

    if (!winner) {
        return {};
    }
    return {
        winner->fidelity,
        winner->reason.empty() ? winner->serialize() : winner->reason,
        true,
    };
}

const std::string& BuildProfile::name() const { return name_; }
const std::vector<ProfileRule>& BuildProfile::rules() const { return rules_; }
UnavailableFidelityPolicy BuildProfile::unavailablePolicy() const { return unavailable_policy_; }

std::string BuildProfile::serialize() const {
    std::ostringstream out;
    out << "name=" << name_ << '\n'
        << "unavailable=" << toString(unavailable_policy_) << '\n';
    for (const auto& rule : rules_) {
        out << "rule=" << rule.serialize() << '\n';
    }
    return out.str();
}

std::string BuildProfile::fingerprint() const {
    // Human-facing profile names and reasons do not change the selected
    // topology. Keep them in serialize(), but exclude them from the layout and
    // build-selection identity.
    std::ostringstream canonical;
    canonical << "unavailable=" << toString(unavailable_policy_) << '\n';
    for (const auto& rule : rules_) {
        canonical << "rule=" << rule.selector.serialize()
                  << "|fidelity=" << toString(rule.fidelity) << '\n';
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16)
        << fnv1a64(canonical.str());
    return out.str();
}

BuildProfileBuilder::BuildProfileBuilder(std::string name)
    : name_(std::move(name)) {}

BuildProfileBuilder& BuildProfileBuilder::addRule(ProfileRule rule) {
    rules_.push_back(std::move(rule));
    return *this;
}

BuildProfileBuilder& BuildProfileBuilder::unavailablePolicy(
    UnavailableFidelityPolicy policy) {
    unavailable_policy_ = policy;
    return *this;
}

BuildProfile BuildProfileBuilder::build() const {
    return BuildProfile(name_, rules_, unavailable_policy_);
}

ProfileRule preferFidelity(Fidelity fidelity, ProfileSelector selector,
                           std::string reason) {
    return {std::move(selector), fidelity, std::move(reason)};
}

BuildProfile withProfileOverrides(
    BuildProfile base,
    std::vector<ProfileRule> overrides,
    std::string profile_name) {
    auto rules = base.rules();
    rules.insert(
        rules.end(),
        std::make_move_iterator(overrides.begin()),
        std::make_move_iterator(overrides.end()));
    return BuildProfile(
        profile_name.empty() ? base.name() + "+overrides" : profile_name,
        std::move(rules),
        base.unavailablePolicy());
}

BuildProfile withExactFidelity(
    BuildProfile base,
    std::string path,
    Fidelity fidelity,
    std::string profile_name) {
    return withProfileOverrides(
        std::move(base),
        {preferFidelity(
            fidelity,
            ProfileSelector::exactPath(std::move(path)),
            "exact profile selection")},
        std::move(profile_name));
}

BuildProfile canonicalDefaultProfile() {
    return BuildProfileBuilder("canonical-default")
        .unavailablePolicy(UnavailableFidelityPolicy::Error)
        .build();
}

} // namespace circuit
