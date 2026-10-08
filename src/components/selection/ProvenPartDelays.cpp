#include "components/selection/ProvenPartDelays.hpp"
#include <map>

namespace circuit {
namespace {

using Arc = PartDelays::Arc;

const std::map<std::string, PartDelays>& tables() {
    static const std::map<std::string, PartDelays> generated = {
#include "ProvenPartDelays.generated.inc"
    };
    return generated;
}

} // namespace

const PartDelays* provenPartDelays(const std::string& implementation_id, const ParameterMap& parameters) {
    // Tables are proven for the default parameters only.
    if (!parameters.empty()) return nullptr;
    const auto found = tables().find(implementation_id);
    return found == tables().end() ? nullptr : &found->second;
}

std::vector<std::string> provenPartDelayImplementations() {
    std::vector<std::string> names;
    for (const auto& [name, table] : tables()) names.push_back(name);
    return names;
}

} // namespace circuit
