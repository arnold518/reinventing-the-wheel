#pragma once
#include "modules/composite/WordComponent.hpp"

// A, B, DIFF, CARRY_IN, OVERFLOW_IN, OP[3] -> OUT. EQ, SLTU, SLT, MINU, MAXU, MINS, MAXS from a subtraction.
template<size_t W> class CompareSelect : public circuit::word::WordComponent {
    static_assert(circuit::word::supportedWidth<W>, "CompareSelect supports 8, 16 and 32 bits");
public:
    explicit CompareSelect(std::string name) : WordComponent(std::move(name), circuit::word::Unit::Compare, W) {}
};

namespace circuit::families {
const ComponentFamily& compareSelect(size_t width);
}
