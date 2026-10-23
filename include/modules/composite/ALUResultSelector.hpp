#pragma once
#include "modules/composite/WordComponent.hpp"

// Six result words and their enables -> OUT, the OR of the enabled words.
template<size_t W> class ALUResultSelector : public circuit::word::WordComponent {
    static_assert(circuit::word::supportedWidth<W>, "ALUResultSelector supports 8, 16 and 32 bits");
public:
    explicit ALUResultSelector(std::string name) : WordComponent(std::move(name), circuit::word::Unit::Result, W) {}
};

namespace circuit::families {
const ComponentFamily& aluResultSelector(size_t width);
}
