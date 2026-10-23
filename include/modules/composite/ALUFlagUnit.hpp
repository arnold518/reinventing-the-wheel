#pragma once
#include "modules/composite/WordComponent.hpp"

// RESULT, ARITHMETIC, SHIFT, ARITH_CARRY, ARITH_OVERFLOW, SHIFT_CARRY -> ZERO, NEGATIVE, CARRY, OVERFLOW.
template<size_t W> class ALUFlagUnit : public circuit::word::WordComponent {
    static_assert(circuit::word::supportedWidth<W>, "ALUFlagUnit supports 8, 16 and 32 bits");
public:
    explicit ALUFlagUnit(std::string name) : WordComponent(std::move(name), circuit::word::Unit::Flags, W) {}
};

namespace circuit::families {
const ComponentFamily& aluFlagUnit(size_t width);
}
