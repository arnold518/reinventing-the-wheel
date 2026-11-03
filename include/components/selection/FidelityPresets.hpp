#pragma once
#include "components/selection/BuildProfile.hpp"
#include <memory>
#include <map>
#include <string_view>
#include <vector>
class Component;
namespace circuit {
class ComponentCatalog;
// Named, versioned ordinary BuildProfiles. Timing and observation policy are
// independent. "scenario" preserves a scenario's existing base profile.
std::vector<std::string> fidelityPresetNames();
std::string visualAbstractionKind(std::string_view contract);
// Contract-level constraints known before building potentially large internals.
std::map<std::string,std::string> fidelityPresetRestrictions(std::string_view contract);
BuildProfile fidelityPreset(std::string_view name,const ComponentCatalog& catalog);
void validateFidelityPreset(const std::shared_ptr<Component>& root,std::string_view name);
}
