#pragma once
#include "modules/composite/WordComponent.hpp"

// A, B, OP[3], C_IN -> OUT, CARRY. SLL, SRL, SRA, ROL, ROR by B mod W.
template<size_t W> class BarrelShifter : public circuit::word::WordComponent {
    static_assert(circuit::word::supportedWidth<W>, "BarrelShifter supports 8, 16 and 32 bits");
public:
    explicit BarrelShifter(std::string name) : WordComponent(std::move(name), circuit::word::Unit::Shift, W) {}
};

namespace circuit::families {
const ComponentFamily& barrelShifter(size_t width);
}
