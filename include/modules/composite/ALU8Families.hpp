#pragma once

#include "components/selection/ComponentFamily.hpp"

// Chapter 2 contracts: structural implementations remain the default.
namespace circuit::families {
extern const ComponentFamily Adder8;
extern const ComponentFamily AddSub8;
extern const ComponentFamily ArithmeticUnit8;
extern const ComponentFamily LogicUnit8;
extern const ComponentFamily BarrelShifter8;
extern const ComponentFamily ZeroDetect8;
extern const ComponentFamily CompareSelect8;
extern const ComponentFamily ALU8FlagUnit;
extern const ComponentFamily ALU8ControlDecoder;
extern const ComponentFamily ALU8ResultSelector;
extern const ComponentFamily ALU8;
} // namespace circuit::families
