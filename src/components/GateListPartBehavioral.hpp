#pragma once
#include "components/selection/ComponentFamily.hpp"
#include "components/IOComponent.hpp"
#include <functional>
namespace circuit::detail {
// Private factory for a compact part that runs its own structural gate list.
std::shared_ptr<Component> createGateListPart(const ComponentFamily&,
    const std::string&,const std::shared_ptr<BuildContext>&);
// Private control groups use the same compiler/publication owner as catalog
// parts. Their reference factory builds the existing structural blueprint.
std::shared_ptr<IOComponent> createGateListControl(const std::string& name,
    const std::string& type, IOComponent::PinInitFunction pins,
    const std::string& identity,
    std::function<std::shared_ptr<IOComponent>()> reference,
    const std::shared_ptr<BuildContext>& context);
}
