#include "components/selection/BuildManifest.hpp"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <unordered_map>

namespace circuit {
namespace {

EffectiveFidelity combine(EffectiveFidelity left, EffectiveFidelity right) {
    if (left == EffectiveFidelity::Unspecified) return right;
    if (right == EffectiveFidelity::Unspecified) return left;
    if (left == right) return left;
    return EffectiveFidelity::Mixed;
}

} // namespace

BuildManifest::BuildManifest(std::string profile_name,
                             std::string profile_fingerprint)
    : profile_name_(std::move(profile_name)),
      profile_fingerprint_(std::move(profile_fingerprint)) {}

void BuildManifest::record(BuildManifestEntry entry) {
    std::lock_guard lock(mutex_);
    entries_.push_back(std::move(entry));
}

void BuildManifest::finalizeEffectiveFidelities() {
    std::lock_guard lock(mutex_);
    std::unordered_map<std::string, size_t> entry_by_path;
    entry_by_path.reserve(entries_.size());

    std::vector<size_t> bottom_up(entries_.size());
    std::iota(bottom_up.begin(), bottom_up.end(), size_t{0});
    for (size_t index = 0; index < entries_.size(); ++index) {
        auto& entry = entries_[index];
        entry.effective_fidelity = entry.selection.fidelity == Fidelity::Structural
            ? EffectiveFidelity::Structural
            : EffectiveFidelity::Behavioral;
        entry_by_path.emplace(entry.path, index);
    }

    std::sort(bottom_up.begin(), bottom_up.end(), [&](size_t left, size_t right) {
        if (entries_[left].depth != entries_[right].depth) {
            return entries_[left].depth > entries_[right].depth;
        }
        return entries_[left].path.size() > entries_[right].path.size();
    });

    for (const auto child_index : bottom_up) {
        auto parent_path = entries_[child_index].path;
        while (true) {
            const auto separator = parent_path.rfind('.');
            if (separator == std::string::npos) {
                break;
            }
            parent_path.resize(separator);
            const auto parent = entry_by_path.find(parent_path);
            if (parent != entry_by_path.end()) {
                auto& parent_fidelity = entries_[parent->second].effective_fidelity;
                parent_fidelity = combine(
                    parent_fidelity, entries_[child_index].effective_fidelity);
                break;
            }
        }
    }
}

std::vector<BuildManifestEntry> BuildManifest::entries() const {
    std::lock_guard lock(mutex_);
    auto result = entries_;
    std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
        return left.path < right.path;
    });
    return result;
}

const std::string& BuildManifest::profileName() const {
    return profile_name_;
}

const std::string& BuildManifest::profileFingerprint() const {
    return profile_fingerprint_;
}

std::string BuildManifest::serialize() const {
    std::ostringstream out;
    out << "profile=" << profile_name_ << '\n';
    out << "fingerprint=" << profile_fingerprint_ << '\n';
    for (const auto& entry : entries()) {
        out << entry.path
            << "|depth=" << entry.depth
            << "|contract=" << entry.selection.contract_id
            << '@' << entry.selection.contract_version
            << "|implementation=" << entry.selection.implementation_id
            << "|fidelity=" << toString(entry.selection.fidelity)
            << "|effective=" << toString(entry.effective_fidelity)
            << "|parameters=" << canonicalizeParameters(entry.parameters)
            << "|reason=" << entry.selection.selection_reason
            << "|exception=" << (entry.selection.used_unavailable_exception ? "true" : "false")
            << '\n';
    }
    return out.str();
}

} // namespace circuit
