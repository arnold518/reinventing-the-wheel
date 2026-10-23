#pragma once
// Private construction and registration of the Chapter 2 word components.
#include "modules/composite/WordComponent.hpp"
#include "modules/composite/ALUControlDecoder.hpp"

namespace circuit { class ComponentCatalog; }
namespace circuit::word {
void build(Unit unit, IOComponent& root, ComponentBuilder& builder, size_t width);
void buildALU(IOComponent&, ComponentBuilder&, size_t width);
void buildAddSub(IOComponent&, ComponentBuilder&, size_t width);
void buildArithmetic(IOComponent&, ComponentBuilder&, size_t width);
void buildLogic(IOComponent&, ComponentBuilder&, size_t width);
void buildShift(IOComponent&, ComponentBuilder&, size_t width);
void buildCompare(IOComponent&, ComponentBuilder&, size_t width);
void buildFlags(IOComponent&, ComponentBuilder&, size_t width);
void buildResult(IOComponent&, ComponentBuilder&, size_t width);
void buildZero(IOComponent&, ComponentBuilder&, size_t width);
void buildRippleAdder(IOComponent&, ComponentBuilder&, size_t width);
void buildPrefixAdder(IOComponent&, ComponentBuilder&, size_t width);
void buildControl(IOComponent&, ComponentBuilder&);

const ComponentFamily& controlFamily();
FamilyFactory behavioralFactory(Unit unit, size_t width);
FamilyFactory controlBehavioralFactory();
void registerComponents(ComponentCatalog& catalog);
}
