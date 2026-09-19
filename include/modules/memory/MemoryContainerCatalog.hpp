#pragma once
#include "components/selection/SelectionTypes.hpp"
#include "modules/memory/LegacyMemoryContainer.hpp"
namespace circuit { class ComponentCatalog; }
MemoryConfig memoryConfigFromParameters(const circuit::ParameterMap& parameters, bool rom = false);
void registerMemoryContainers(circuit::ComponentCatalog& catalog);
void registerFFMemoryWords(circuit::ComponentCatalog& catalog);
