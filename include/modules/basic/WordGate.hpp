#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <string>
namespace circuit {class ComponentCatalog;}

// A one-bit gate applied to every bit of a word: A, B -> OUT (NOT: IN -> OUT),
// all W bits wide. The structural form is W one-bit gates between zero-delay
// bit Rewires, the same gates and depth as a recipe that applies the gate bit
// by bit. The compact form evaluates the word at once on the delays proven on
// those gates.
namespace circuit::word {
enum class GateKind { And, Nand, Or, Nor, Xor, Not };
// Every width from 2 to 32 bits (2026-10-28: groups of 5, 6, 30 or 31 bits
// regroup too); one bit is the plain gate.
inline constexpr bool gateWidth(size_t width) { return width >= 2 && width <= 32; }
// Contract `logic.<kind>.<W>`, type `<KIND>Gate<W>` (for example ANDGate<32>).
const ComponentFamily& gate(GateKind kind, size_t width);
void registerGates(ComponentCatalog&);

class WordGate : public IOComponent {
    GateKind kind_;
    size_t width_;
    std::string type_;
public:
    WordGate(std::string name, GateKind kind, size_t width);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
};
}

namespace circuit::families {
inline const ComponentFamily& wordGate(word::GateKind kind, size_t width) { return word::gate(kind, width); }
}
