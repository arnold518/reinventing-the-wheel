#include "components/selection/BuildManifest.hpp"
#include <algorithm>
#include <sstream>

namespace circuit {

BuildManifest::BuildManifest(std::string profile_name,
                             std::string profile_fingerprint)
    : profile_name_(std::move(profile_name)),
      profile_fingerprint_(std::move(profile_fingerprint)) {}

void BuildManifest::record(BuildManifestEntry entry) {
    std::lock_guard lock(mutex_);
    entries_.push_back(std::move(entry));
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
            << "|parameters=" << canonicalizeParameters(entry.parameters)
            << "|reason=" << entry.selection.selection_reason
            << "|exception=" << (entry.selection.used_unavailable_exception ? "true" : "false")
            << '\n';
    }
    return out.str();
}

} // namespace circuit
