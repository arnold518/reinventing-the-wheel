#pragma once

#include "components/selection/BuildProfile.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "components/selection/SelectionTypes.hpp"
#include <string>

namespace circuit {
class ComponentCatalog;
}

namespace rv32i {

circuit::ComponentBuildRequest educationalSystemRequest(std::string instance_name);

circuit::BuildProfile balancedSystemProfile(
    const circuit::ComponentCatalog& catalog,
    const circuit::ComponentBuildRequest& root);

} // namespace rv32i
