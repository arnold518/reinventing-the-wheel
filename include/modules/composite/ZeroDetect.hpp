#pragma once
#include "modules/composite/WordComponent.hpp"

// A -> ZERO.
template<size_t W> class ZeroDetect : public circuit::word::WordComponent {
    static_assert(circuit::word::supportedWidth<W>, "ZeroDetect supports 8, 16 and 32 bits");
public:
    explicit ZeroDetect(std::string name) : WordComponent(std::move(name), circuit::word::Unit::Zero, W) {}
};

namespace circuit::families {
const ComponentFamily& zeroDetect(size_t width);
}
