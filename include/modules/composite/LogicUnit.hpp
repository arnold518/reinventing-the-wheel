#pragma once
#include "modules/composite/WordComponent.hpp"

// A, B, OP[3] -> OUT. AND, OR, XOR, NOT, NAND, NOR, XNOR.
template<size_t W> class LogicUnit : public circuit::word::WordComponent {
    static_assert(circuit::word::supportedWidth<W>, "LogicUnit supports 8, 16 and 32 bits");
public:
    explicit LogicUnit(std::string name) : WordComponent(std::move(name), circuit::word::Unit::Logic, W) {}
};

namespace circuit::families {
const ComponentFamily& logicUnit(size_t width);
}
