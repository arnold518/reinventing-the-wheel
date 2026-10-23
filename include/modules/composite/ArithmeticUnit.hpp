#pragma once
#include "modules/composite/WordComponent.hpp"

// A, B, OP[3], C_IN -> OUT, CARRY, OVERFLOW, INVALID_OP. ADD, ADC, SUB, SBC, INC, DEC, NEG on one add/subtract path.
template<size_t W> class ArithmeticUnit : public circuit::word::WordComponent {
    static_assert(circuit::word::supportedWidth<W>, "ArithmeticUnit supports 8, 16 and 32 bits");
public:
    explicit ArithmeticUnit(std::string name) : WordComponent(std::move(name), circuit::word::Unit::Arithmetic, W) {}
};

namespace circuit::families {
const ComponentFamily& arithmeticUnit(size_t width);
}
