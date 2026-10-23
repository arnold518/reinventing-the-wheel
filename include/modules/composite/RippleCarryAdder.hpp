#pragma once
#include "modules/composite/WordComponent.hpp"

// A, B, C_IN -> OUT, C_OUT through W chained full adders.
template<size_t W> class RippleCarryAdder : public circuit::word::WordComponent {
    static_assert(circuit::word::supportedWidth<W>, "RippleCarryAdder supports 8, 16 and 32 bits");
public:
    explicit RippleCarryAdder(std::string name) : WordComponent(std::move(name), circuit::word::Unit::RippleAdder, W) {}
};

namespace circuit::families {
const ComponentFamily& rippleCarryAdder(size_t width);
}
