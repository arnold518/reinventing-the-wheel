#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

// Shared base of the Chapter 2 word components. Every unit is built by one
// width-driven recipe at W = 8, 16 or 32; the public classes below select a unit.
namespace circuit::word {
enum class Unit {
    ALU, AddSub, Arithmetic, Logic, Shift, Compare, Flags, Result, RippleAdder, PrefixAdder, Zero
};
// Catalog family of one unit at one width.
const ComponentFamily& family(Unit unit, size_t width);

class WordComponent : public IOComponent {
    Unit unit_;
    size_t width_;
    std::string type_;
public:
    WordComponent(std::string name, Unit unit, size_t width);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& builder) override;
};

template<size_t W> inline constexpr bool supportedWidth = W == 8 || W == 16 || W == 32;
}
