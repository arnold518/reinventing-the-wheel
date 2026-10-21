#pragma once
#include "components/selection/ComponentFamily.hpp"
namespace circuit::detail {
// Private factory for a compact part that runs its own structural gate list.
std::shared_ptr<Component> createGateListPart(const ComponentFamily&,
    const std::string&,const std::shared_ptr<BuildContext>&);
}
