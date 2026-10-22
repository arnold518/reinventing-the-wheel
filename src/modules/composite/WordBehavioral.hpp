#pragma once
#include "modules/composite/WordDatapath.hpp"
namespace circuit::word {
FamilyFactory behavioralFactory(Unit,size_t width,ALUConfig);
FamilyFactory controlBehavioralFactory(Operations);
}
