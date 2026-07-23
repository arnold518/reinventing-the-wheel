#pragma once

#include "components/selection/ComponentCatalog.hpp"
#include <memory>

namespace circuit {

std::shared_ptr<ComponentCatalog> createBuiltinComponentCatalog();
const ComponentCatalog& builtinComponentCatalog();

} // namespace circuit
