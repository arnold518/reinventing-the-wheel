#pragma once

#include "components/selection/SelectionTypes.hpp"
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

namespace circuit {

struct BuildManifestEntry {
    std::string path;
    size_t depth = 0;
    ParameterMap parameters;
    ResolvedSelection selection;
};

class BuildManifest {
public:
    BuildManifest(std::string profile_name, std::string profile_fingerprint);

    void record(BuildManifestEntry entry);

    std::vector<BuildManifestEntry> entries() const;
    const std::string& profileName() const;
    const std::string& profileFingerprint() const;
    std::string serialize() const;

private:
    std::string profile_name_;
    std::string profile_fingerprint_;
    mutable std::mutex mutex_;
    std::vector<BuildManifestEntry> entries_;
};

} // namespace circuit
