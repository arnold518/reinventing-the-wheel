#include "components/selection/BuildProfile.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <algorithm>
#include <iomanip>
#include <limits>
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

size_t ProfileSelector::specificity() const {
    size_t score = 0;
    if (exact_path) score += 1'000'000 + exact_path->size();
    if (subtree_path) score += 100'000 + subtree_path->size();
    if (contract_id) score += 10'000;
    score += parameters.size() * 1'000;
    if (minimum_depth || maximum_depth) score += 100;
    return score;
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
        << "|priority=" << priority
        << "|reason=" << reason;
    return out.str();
}

BuildProfile::BuildProfile(std::string name, std::vector<ProfileRule> rules,
                           UnavailableFidelityPolicy unavailable_policy,
                           bool allow_reference,
                           std::string generator_descriptor)
    : name_(std::move(name)),
      rules_(std::move(rules)),
      unavailable_policy_(unavailable_policy),
      allow_reference_(allow_reference),
      generator_descriptor_(std::move(generator_descriptor)) {
    if (name_.empty()) {
        throw std::invalid_argument("BuildProfile name must not be empty");
    }
}

ProfileDecision BuildProfile::decide(const std::string& path, size_t depth,
                                     const std::string& contract_id,
                                     const ParameterMap& parameters) const {
    const ProfileRule* winner = nullptr;
    size_t winning_specificity = 0;
    int winning_priority = std::numeric_limits<int>::min();

    for (const auto& rule : rules_) {
        if (!rule.selector.matches(path, depth, contract_id, parameters)) {
            continue;
        }
        const size_t specificity = rule.selector.specificity();
        if (!winner || specificity > winning_specificity
            || (specificity == winning_specificity && rule.priority > winning_priority)) {
            winner = &rule;
            winning_specificity = specificity;
            winning_priority = rule.priority;
            continue;
        }
        if (specificity == winning_specificity && rule.priority == winning_priority
            && rule.fidelity != winner->fidelity) {
            throw std::runtime_error("Conflicting profile rules for path '" + path + "'");
        }
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
bool BuildProfile::allowReference() const { return allow_reference_; }
const std::string& BuildProfile::generatorDescriptor() const { return generator_descriptor_; }

std::string BuildProfile::serialize() const {
    std::ostringstream out;
    out << "name=" << name_ << '\n'
        << "unavailable=" << toString(unavailable_policy_) << '\n'
        << "allow_reference=" << (allow_reference_ ? "true" : "false") << '\n'
        << "generator=" << generator_descriptor_ << '\n';
    for (const auto& rule : rules_) {
        out << "rule=" << rule.serialize() << '\n';
    }
    return out.str();
}

std::string BuildProfile::fingerprint() const {
    std::ostringstream out;
    out << std::hex << std::setfill('0') << std::setw(16) << fnv1a64(serialize());
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

BuildProfileBuilder& BuildProfileBuilder::allowReference(bool allow) {
    allow_reference_ = allow;
    return *this;
}

BuildProfileBuilder& BuildProfileBuilder::descriptor(std::string descriptor) {
    descriptor_ = std::move(descriptor);
    return *this;
}

BuildProfile BuildProfileBuilder::build() const {
    return BuildProfile(name_, rules_, unavailable_policy_, allow_reference_, descriptor_);
}

ProfileRule preferFidelity(Fidelity fidelity, ProfileSelector selector,
                           std::string reason) {
    return {std::move(selector), fidelity, 0, std::move(reason)};
}

} // namespace circuit
