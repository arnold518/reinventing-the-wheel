#pragma once

#include "components/StructuralNetBuilder.hpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <array>

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
    CarryPrefixLevel(std::string name, size_t distance,size_t width=32);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override;
private:
    size_t distance_,width_;
    std::string type_;
};
}

namespace circuit::word {
class PrefixWord : public IOComponent {
    size_t width_;std::string type_;
public:PrefixWord(std::string,size_t width);
    const char* getTypeName()const override{return type_.c_str();}
    void buildInternals(ComponentBuilder&)override;
};
}
template<size_t W>class ParallelPrefixAdder : public circuit::word::PrefixWord {
    static_assert(W==8 || W==16 || W==32);
public:explicit ParallelPrefixAdder(std::string n):PrefixWord(std::move(n),W){}
};
