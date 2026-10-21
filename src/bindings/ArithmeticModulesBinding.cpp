#include "ModuleBindingHelpers.hpp"
#include "modules/composite/Adder8.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/composite/ALU8.hpp"
#include "modules/composite/ALU8ControlDecoder.hpp"
#include "modules/composite/ALU8ResultSelector.hpp"
#include "modules/composite/ALU32ControlDecoder.hpp"
#include "modules/composite/ALU32CompareUnit.hpp"
#include "modules/composite/ALU32ResultSelector.hpp"
#include "modules/composite/ALU32FlagUnit.hpp"
#include "modules/composite/AddSub8.hpp"
#include "modules/composite/ArithmeticUnit8.hpp"
#include "modules/composite/LogicUnit8.hpp"
#include "modules/composite/BarrelShifter8.hpp"
#include "modules/composite/CompareSelect8.hpp"
#include "modules/composite/ALU8FlagUnit.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/composite/Shifter32.hpp"
#include "modules/composite/ALU32.hpp"

void bindArithmeticModules(py::module_& m) {
    bindCompositeModule<Adder8>(m, "Adder8", "8-bit adder.");
    bindCompositeModule<ZeroDetect8>(m, "ZeroDetect8", "8-bit zero detector.");
    bindCompositeModule<ALU8>(m, "ALU8", "8-bit arithmetic logic unit.");
    bindCompositeModule<ALU8ControlDecoder>(m, "ALU8ControlDecoder", "Structural ALU integration block.");
    bindCompositeModule<ALU8ResultSelector>(m, "ALU8ResultSelector", "Structural ALU integration block.");
    bindCompositeModule<ALU32ControlDecoder>(m, "ALU32ControlDecoder", "Structural ALU integration block.");
    bindCompositeModule<ALU32CompareUnit>(m, "ALU32CompareUnit", "Structural ALU integration block.");
    bindCompositeModule<ALU32ResultSelector>(m, "ALU32ResultSelector", "Structural ALU integration block.");
    bindCompositeModule<ALU32FlagUnit>(m, "ALU32FlagUnit", "Structural ALU integration block.");
    bindCompositeModule<AddSub8>(m, "AddSub8", "8-bit structural add/sub with independent carry input.");
    bindCompositeModule<ArithmeticUnit8>(m, "ArithmeticUnit8", "Shared 8-bit arithmetic unit.");
    bindCompositeModule<LogicUnit8>(m, "LogicUnit8", "8-bit structural logic unit.");
    bindCompositeModule<BarrelShifter8>(m, "BarrelShifter8", "8-bit variable shifts and rotations.");
    bindCompositeModule<CompareSelect8>(m, "CompareSelect8", "Predicates and min/max from shared subtraction.");
    bindCompositeModule<ALU8FlagUnit>(m, "ALU8FlagUnit", "8-bit ALU result and arithmetic flags.");
    bindCompositeModule<Adder32>(m, "Adder32", "Structural 32-bit adder.");
    bindCompositeModule<AddSub32>(m, "AddSub32", "Structural 32-bit add/subtract unit.");
    bindCompositeModule<Logic32>(m, "Logic32", "Structural 32-bit bitwise logic unit.");
    bindCompositeModule<ZeroDetect32>(m, "ZeroDetect32", "Structural 32-bit zero detector.");
    bindCompositeModule<Shifter32>(m, "Shifter32", "Structural 32-bit barrel shifter.");
    bindCompositeModule<ALU32>(m, "ALU32", "Structural RV32I-oriented 32-bit ALU.");


}
