#pragma once

#include "modules/composite/WordComponent.hpp"

namespace circuit::arithmetic_detail {
// Composition of two carry transformations c' = G | (P & c).
// AND/OR distributivity preserves the ripple circuit's four-state behavior.
class CarryCombine : public IOComponent {
public:
    explicit CarryCombine(std::string name);
    const char* getTypeName() const override { return "CarryCombine"; }
    void buildInternals(ComponentBuilder& b) override;
};

class CarryPrefixLevel : public IOComponent {
public:
    CarryPrefixLevel(std::string name, size_t distance, size_t width = 32);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override;
private:
    size_t distance_, width_;
    std::string type_;
};
}

// A, B, C_IN -> OUT, C_OUT, C_SIGN (the carry into bit W-1).
// log2(W) carry-combination levels instead of a W-stage chain.
template<size_t W> class ParallelPrefixAdder : public circuit::word::WordComponent {
    static_assert(circuit::word::supportedWidth<W>, "ParallelPrefixAdder supports 8, 16 and 32 bits");
public:
    explicit ParallelPrefixAdder(std::string name)
        : WordComponent(std::move(name), circuit::word::Unit::PrefixAdder, W) {}
};

namespace circuit::families {
const ComponentFamily& parallelPrefixAdder(size_t width);
}
