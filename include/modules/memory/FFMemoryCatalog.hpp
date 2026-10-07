#pragma once
#include "components/selection/SelectionTypes.hpp"
#include "modules/memory/MemoryArrayInterface.hpp"
namespace circuit { class ComponentCatalog; }
MemoryConfig ffMemoryConfigFromParameters(const circuit::ParameterMap& parameters);
void registerFFMemories(circuit::ComponentCatalog& catalog);
void registerFFMemoryWords(circuit::ComponentCatalog& catalog);
