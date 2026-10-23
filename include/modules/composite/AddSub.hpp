#pragma once
#include "modules/composite/WordComponent.hpp"

// A, B, SUB, C_IN -> OUT, C_OUT, OVERFLOW. OUT = A + (B XOR SUB) + C_IN, using the parallel-prefix adder.
template<size_t W> class AddSub : public circuit::word::WordComponent {
    static_assert(circuit::word::supportedWidth<W>, "AddSub supports 8, 16 and 32 bits");
public:
    explicit AddSub(std::string name) : WordComponent(std::move(name), circuit::word::Unit::AddSub, W) {}
};

namespace circuit::families {
const ComponentFamily& addSub(size_t width);
}
