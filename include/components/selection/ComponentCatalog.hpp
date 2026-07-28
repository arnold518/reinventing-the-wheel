#pragma once

#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuildProfile.hpp"
#include "components/selection/SelectionTypes.hpp"
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Component;

namespace circuit {

class BuildContext;

using ParameterPredicate = std::function<bool(const ParameterMap&)>;
using ComponentFactory = std::function<std::shared_ptr<Component>(
    const std::string&, const ParameterMap&, const std::shared_ptr<BuildContext>&)>;

struct ImplementationDescriptor {
    std::string id;
    std::string contract_id;
    Fidelity fidelity = Fidelity::Structural;
    bool terminal_primitive = false;
    std::string concrete_type_name;
    std::vector<std::string> capabilities;
    VerificationEvidence evidence;
    ParameterPredicate supports;
    ComponentFactory factory;
};

struct BuildResult {
    std::shared_ptr<Component> root;
    std::shared_ptr<const BuildProfile> profile;
    std::shared_ptr<BuildManifest> manifest;
};

class ComponentCatalog {
public:
    void registerContract(ContractDescriptor descriptor);
    void registerImplementation(ImplementationDescriptor descriptor);
    void freeze();

    bool frozen() const;
    bool hasContract(const std::string& id) const;
    const ContractDescriptor& contract(const std::string& id) const;
    const ImplementationDescriptor& implementation(const std::string& id) const;
    std::vector<ContractDescriptor> contracts() const;
    std::vector<ImplementationDescriptor> implementationsFor(
        const std::string& contract_id) const;

    ResolvedSelection resolve(const ComponentBuildRequest& request,
                              const std::string& path,
                              size_t depth,
                              const BuildProfile& profile) const;

    std::shared_ptr<Component> createChild(
        const ComponentBuildRequest& request,
        const std::shared_ptr<BuildContext>& parent_context) const;

    BuildResult createRoot(ComponentBuildRequest request,
                           BuildProfile profile) const;

private:
    std::vector<const ImplementationDescriptor*> candidatesFor(
        const ComponentBuildRequest& request) const;
    const ImplementationDescriptor& descriptorForSelection(
        const ResolvedSelection& selection) const;
    void validateInstance(const ContractDescriptor& contract,
                          const ImplementationDescriptor& implementation,
                          const std::shared_ptr<Component>& component) const;

    std::map<std::string, ContractDescriptor> contracts_;
    std::map<std::string, ImplementationDescriptor> implementations_;
    bool frozen_ = false;
};

} // namespace circuit
