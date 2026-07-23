#include "components/selection/BuildContext.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuildProfile.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include <stdexcept>

namespace circuit {

BuildContext::BuildContext(const ComponentCatalog& catalog,
                           std::shared_ptr<const BuildProfile> profile,
                           std::shared_ptr<BuildManifest> manifest,
                           std::string path,
                           size_t depth,
                           bool has_component_path,
                           std::optional<ResolvedSelection> selection,
                           ParameterMap parameters)
    : catalog_(&catalog),
      profile_(std::move(profile)),
      manifest_(std::move(manifest)),
      path_(std::move(path)),
      depth_(depth),
      has_component_path_(has_component_path),
      selection_(std::move(selection)),
      parameters_(std::move(parameters)) {}

std::shared_ptr<BuildContext> BuildContext::rootScope(
    const ComponentCatalog& catalog,
    std::shared_ptr<const BuildProfile> profile,
    std::shared_ptr<BuildManifest> manifest) {
    if (!profile || !manifest) {
        throw std::invalid_argument("BuildContext root scope requires profile and manifest");
    }
    return std::shared_ptr<BuildContext>(new BuildContext(
        catalog, std::move(profile), std::move(manifest), "", 0, false,
        std::nullopt, ParameterMap{}));
}

std::shared_ptr<BuildContext> BuildContext::child(
    std::string instance_name,
    std::optional<ResolvedSelection> selection,
    ParameterMap parameters) const {
    return std::shared_ptr<BuildContext>(new BuildContext(
        *catalog_, profile_, manifest_, childPath(instance_name), childDepth(), true,
        std::move(selection), std::move(parameters)));
}

const ComponentCatalog& BuildContext::catalog() const { return *catalog_; }
const BuildProfile& BuildContext::profile() const { return *profile_; }
const std::shared_ptr<BuildManifest>& BuildContext::manifest() const { return manifest_; }
const std::string& BuildContext::path() const { return path_; }
size_t BuildContext::depth() const { return depth_; }
bool BuildContext::hasComponentPath() const { return has_component_path_; }
const std::optional<ResolvedSelection>& BuildContext::selection() const { return selection_; }
const ParameterMap& BuildContext::parameters() const { return parameters_; }

std::string BuildContext::childPath(const std::string& instance_name) const {
    return has_component_path_ ? path_ + "." + instance_name : instance_name;
}

size_t BuildContext::childDepth() const {
    return has_component_path_ ? depth_ + 1 : 0;
}

void BuildContext::applyMetadata(Component& component) const {
    if (!selection_) {
        return;
    }
    component.setInstanceMetadata(ComponentInstanceMetadata{
        selection_->contract_id,
        selection_->contract_version,
        selection_->implementation_id,
        selection_->fidelity,
        selection_->terminal_primitive,
        selection_->reference_only,
        selection_->used_unavailable_exception,
        selection_->selection_reason,
        profile_->fingerprint(),
    });
}

void BuildContext::recordCreated(const Component& component) const {
    ResolvedSelection recorded;
    if (selection_) {
        recorded = *selection_;
    } else {
        // Unselected children are implementation details created by a selected
        // structural blueprint. A directly evaluated child is the chosen
        // structural terminal for that blueprint; this says nothing about its
        // concrete C++ class name.
        const bool primitive =
            dynamic_cast<const BasicComponent*>(&component) != nullptr;
        const bool io = dynamic_cast<const IOComponent*>(&component) != nullptr;
        recorded.contract_id = "explicit." + std::string(component.getTypeName());
        recorded.implementation_id = "explicit.construction." + std::string(component.getTypeName());
        recorded.fidelity = Fidelity::Structural;
        recorded.terminal_primitive = primitive;
        recorded.selection_reason = io
            ? "internal detail of selected structural implementation"
            : "explicit construction through general component base";
    }

    manifest_->record(BuildManifestEntry{
        path_, depth_, parameters_, std::move(recorded), EffectiveFidelity::Unspecified,
    });
}

} // namespace circuit
