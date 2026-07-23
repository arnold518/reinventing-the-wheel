#pragma once

#include "components/selection/SelectionTypes.hpp"
#include <memory>
#include <optional>
#include <string>

class Component;

namespace circuit {

class BuildManifest;
class BuildProfile;
class ComponentCatalog;

class BuildContext : public std::enable_shared_from_this<BuildContext> {
public:
    static std::shared_ptr<BuildContext> rootScope(
        const ComponentCatalog& catalog,
        std::shared_ptr<const BuildProfile> profile,
        std::shared_ptr<BuildManifest> manifest);

    std::shared_ptr<BuildContext> child(
        std::string instance_name,
        std::optional<ResolvedSelection> selection,
        ParameterMap parameters = {}) const;

    const ComponentCatalog& catalog() const;
    const BuildProfile& profile() const;
    const std::shared_ptr<BuildManifest>& manifest() const;
    const std::string& path() const;
    size_t depth() const;
    bool hasComponentPath() const;
    const std::optional<ResolvedSelection>& selection() const;
    const ParameterMap& parameters() const;

    std::string childPath(const std::string& instance_name) const;
    size_t childDepth() const;
    void applyMetadata(Component& component) const;
    void recordCreated(const Component& component) const;

private:
    BuildContext(const ComponentCatalog& catalog,
                 std::shared_ptr<const BuildProfile> profile,
                 std::shared_ptr<BuildManifest> manifest,
                 std::string path,
                 size_t depth,
                 bool has_component_path,
                 std::optional<ResolvedSelection> selection,
                 ParameterMap parameters);

    const ComponentCatalog* catalog_;
    std::shared_ptr<const BuildProfile> profile_;
    std::shared_ptr<BuildManifest> manifest_;
    std::string path_;
    size_t depth_ = 0;
    bool has_component_path_ = false;
    std::optional<ResolvedSelection> selection_;
    ParameterMap parameters_;
};

} // namespace circuit
