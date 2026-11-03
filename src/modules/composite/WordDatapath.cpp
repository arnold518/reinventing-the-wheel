#include "modules/composite/WordRecipes.hpp"
#include "modules/composite/ALU.hpp"
#include "modules/composite/AddSub.hpp"
#include "modules/composite/ArithmeticUnit.hpp"
#include "modules/composite/LogicUnit.hpp"
#include "modules/composite/BarrelShifter.hpp"
#include "modules/composite/CompareSelect.hpp"
#include "modules/composite/ALUFlagUnit.hpp"
#include "modules/composite/ALUResultSelector.hpp"
#include "modules/composite/ZeroDetect.hpp"
#include "modules/composite/RippleCarryAdder.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/FamilyRegistration.hpp"
#include <array>
#include <map>
#include <stdexcept>

namespace circuit::word {
namespace {
void validate(size_t width) {
    if (width != 8 && width != 16 && width != 32)
        throw std::invalid_argument("Word components support 8, 16 and 32 bits");
}
// Concrete type name and contract-ID stem of each unit.
std::pair<const char*, const char*> names(Unit u) {
    switch (u) {
    case Unit::ALU: return {"ALU", "alu"};
    case Unit::AddSub: return {"AddSub", "add-sub"};
    case Unit::Arithmetic: return {"ArithmeticUnit", "arithmetic-unit"};
    case Unit::Logic: return {"LogicUnit", "logic-unit"};
    case Unit::Shift: return {"BarrelShifter", "barrel-shifter"};
    case Unit::Compare: return {"CompareSelect", "compare-select"};
    case Unit::Flags: return {"ALUFlagUnit", "alu-flag-unit"};
    case Unit::Result: return {"ALUResultSelector", "alu-result-selector"};
    case Unit::RippleAdder: return {"RippleCarryAdder", "ripple-carry-adder"};
    case Unit::PrefixAdder: return {"ParallelPrefixAdder", "parallel-prefix-adder"};
    case Unit::Zero: return {"ZeroDetect", "zero-detect"};
    }
    throw std::invalid_argument("Invalid word unit");
}
std::string typeName(Unit u, size_t w) { return std::string(names(u).first) + "<" + std::to_string(w) + ">"; }
std::string contractId(Unit u, size_t w) { return std::string("word.") + names(u).second + "." + std::to_string(w); }

void pins(IOComponent* p, Unit u, size_t w) {
    validate(w);
    auto input = [&](const char* n, size_t width = 1) { p->addPinDynamic(n, PinType::INPUT, width); };
    auto output = [&](const char* n, size_t width = 1) { p->addPinDynamic(n, PinType::OUTPUT, width); };
    if (u == Unit::Zero) { input("A", w); output("ZERO"); return; }
    if (u == Unit::Flags) {
        input("RESULT", w);
        for (auto n : {"ARITHMETIC", "SHIFT", "ARITH_CARRY", "ARITH_OVERFLOW", "SHIFT_CARRY"}) input(n);
        for (auto n : {"ZERO", "NEGATIVE", "CARRY", "OVERFLOW"}) output(n);
        return;
    }
    input("A", w); input("B", w); output("OUT", w);
    switch (u) {
    case Unit::RippleAdder: input("C_IN"); output("C_OUT"); return;
    case Unit::PrefixAdder: input("C_IN"); output("C_OUT"); output("C_SIGN"); return;
    case Unit::AddSub: input("SUB"); input("C_IN"); output("C_OUT"); output("OVERFLOW"); return;
    case Unit::Result:
        for (auto n : {"ARITHMETIC_RESULT", "LOGIC_RESULT", "SHIFT_RESULT", "COMPARE_RESULT"}) input(n, w);
        for (auto n : {"ARITHMETIC_ENABLE", "LOGIC_ENABLE", "SHIFT_ENABLE", "COMPARE_ENABLE", "PASS_A_ENABLE", "PASS_B_ENABLE"}) input(n);
        return;
    case Unit::Logic: input("OP", 3); return;
    case Unit::Compare: input("OP", 3); input("DIFF", w); input("CARRY_IN"); input("OVERFLOW_IN"); return;
    case Unit::Shift: input("OP", 3); input("C_IN"); output("CARRY"); return;
    case Unit::Arithmetic: input("OP", 3); input("C_IN"); output("CARRY"); output("OVERFLOW"); output("INVALID_OP"); return;
    case Unit::ALU:
        input("OP", 5); input("C_IN");
        for (auto n : {"CARRY", "OVERFLOW", "INVALID_OP", "ZERO", "NEGATIVE"}) output(n);
        return;
    default: throw std::invalid_argument("Invalid word unit");
    }
}

struct Entry {
    // ComponentFamily keeps views of these strings, so they live here.
    std::string id, type;
    ComponentFamily family;
    Entry(Unit u, size_t w)
        : id(contractId(u, w)), type(typeName(u, w)),
          family(id, type, [u, w](IOComponent* p) { pins(p, u, w); },
              [u, w](const std::string& n, const std::shared_ptr<BuildContext>& context) {
                  return Component::createWithContext<WordComponent>(context, n, u, w);
              },
              behavioralFactory(u, w)) {}
};

constexpr std::array<Unit, 11> all_units{Unit::ALU, Unit::AddSub, Unit::Arithmetic, Unit::Logic, Unit::Shift,
    Unit::Compare, Unit::Flags, Unit::Result, Unit::RippleAdder, Unit::PrefixAdder, Unit::Zero};

// The logical test that owns each family's contract. The recorded ALU<W> and
// 16-bit adder scenarios are grouped under ALUWidthContractTest, so that test
// owns those contracts; a contract must have exactly one logical owner.
std::string contractOwner(Unit u, size_t width) {
    if (width == 8) {
        switch (u) {
        case Unit::ALU: return "ALUWidthContractTest";
        case Unit::AddSub: return "AddSubWidth8Test";
        case Unit::Arithmetic: return "ArithmeticUnitWidth8Test";
        case Unit::Logic: return "LogicUnitWidth8Test";
        case Unit::Shift: return "BarrelShifterWidth8Test";
        case Unit::Compare: return "CompareSelectWidth8Test";
        case Unit::Flags: return "ALUFlagUnitWidth8Test";
        case Unit::Result: return "ALUResultSelectorWidth8Test";
        case Unit::RippleAdder: return "RippleCarryAdderWidth8Test";
        case Unit::PrefixAdder: return "ALUFidelityContractTest";
        case Unit::Zero: return "ZeroDetectWidth8Test";
        default: break;
        }
    }
    if (width == 32 && u == Unit::PrefixAdder) return "ParallelPrefixAdderWidth32Test";
    if (width == 32 && u == Unit::Zero) return "ZeroDetectWidth32Test";
    if (width == 32 && u == Unit::RippleAdder) return "AdderWidthSweepTest";
    // The gate-level ALU<W> drives every 16/32-bit sub-unit through an edge grid
    // and random vectors against an independent integer reference.
    return "ALUWidthContractTest";
}

// The test that compares a compact form with its gates.
std::string equivalenceOwner(Unit u, size_t width) {
    if (u == Unit::ALU && width == 8) return "ALUFidelityContractTest";
    // Both adders at 16/32 bits are swept against a + b + C_IN in both forms.
    if ((u == Unit::RippleAdder || u == Unit::PrefixAdder) && width != 8) return "AdderWidthSweepTest";
    if (width == 32 && u == Unit::Zero) return "ZeroDetectWidth32Test";
    // The other 16/32-bit compact forms (2026-10-28) run beside their gates on
    // binary and four-state vectors.
    if (width != 8) return "CompactFormEquivalenceTest";
    return contractOwner(u, width);
}
}

const ComponentFamily& family(Unit u, size_t width) {
    validate(width);
    static std::map<std::pair<Unit, size_t>, std::unique_ptr<Entry>> entries;
    auto& entry = entries[{u, width}];
    if (!entry) entry = std::make_unique<Entry>(u, width);
    return entry->family;
}

WordComponent::WordComponent(std::string name, Unit u, size_t width)
    : IOComponent(std::move(name), [u, width](IOComponent* p) { pins(p, u, width); }),
      unit_(u), width_(width), type_(typeName(u, width)) {}

void WordComponent::buildInternals(ComponentBuilder& b) { build(unit_, *this, b, width_); }

void build(Unit u, IOComponent& self, ComponentBuilder& b, size_t width) {
    validate(width);
    switch (u) {
    case Unit::ALU: buildALU(self, b, width); return;
    case Unit::AddSub: buildAddSub(self, b, width); return;
    case Unit::Arithmetic: buildArithmetic(self, b, width); return;
    case Unit::Logic: buildLogic(self, b, width); return;
    case Unit::Shift: buildShift(self, b, width); return;
    case Unit::Compare: buildCompare(self, b, width); return;
    case Unit::Flags: buildFlags(self, b, width); return;
    case Unit::Result: buildResult(self, b, width); return;
    case Unit::RippleAdder: buildRippleAdder(self, b, width); return;
    case Unit::PrefixAdder: buildPrefixAdder(self, b, width); return;
    case Unit::Zero: buildZero(self, b, width); return;
    }
    throw std::invalid_argument("Invalid word unit");
}

void registerComponents(ComponentCatalog& catalog) {
    const auto& control = controlFamily();
    registration::registerStructural(catalog, control, "ALUControlDecoderTest");
    registration::registerBehavioral(catalog, control, "ALUControlDecoderTest", "ALUControlDecoderTest", {}, "stable-after-settle");
    for (size_t width : {8u, 16u, 32u})
        for (auto u : all_units) {
            const auto& f = family(u, width);
            const auto test = contractOwner(u, width);
            registration::registerStructural(catalog, f, test, "stable-after-settle");
            if (f.supports(Fidelity::Behavioral))
                registration::registerBehavioral(catalog, f, test, equivalenceOwner(u, width), {}, "stable-after-settle");
        }
}
}

namespace circuit::families {
using circuit::word::Unit;
const ComponentFamily& alu(size_t w) { return word::family(Unit::ALU, w); }
const ComponentFamily& addSub(size_t w) { return word::family(Unit::AddSub, w); }
const ComponentFamily& arithmeticUnit(size_t w) { return word::family(Unit::Arithmetic, w); }
const ComponentFamily& logicUnit(size_t w) { return word::family(Unit::Logic, w); }
const ComponentFamily& barrelShifter(size_t w) { return word::family(Unit::Shift, w); }
const ComponentFamily& compareSelect(size_t w) { return word::family(Unit::Compare, w); }
const ComponentFamily& aluFlagUnit(size_t w) { return word::family(Unit::Flags, w); }
const ComponentFamily& aluResultSelector(size_t w) { return word::family(Unit::Result, w); }
const ComponentFamily& zeroDetect(size_t w) { return word::family(Unit::Zero, w); }
const ComponentFamily& rippleCarryAdder(size_t w) { return word::family(Unit::RippleAdder, w); }
const ComponentFamily& parallelPrefixAdder(size_t w) { return word::family(Unit::PrefixAdder, w); }
const ComponentFamily& aluControlDecoder() { return word::controlFamily(); }
}
