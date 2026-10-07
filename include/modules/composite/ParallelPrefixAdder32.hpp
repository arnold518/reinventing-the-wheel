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
    CarryPrefixLevel(std::string name, size_t distance);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override;
private:
    size_t distance_;
    std::string type_;
};
}

// Gate-built five-level parallel-prefix adder. The public wrappers retain
// their original pins; C31 is exposed here for AddSub32 signed overflow.
class ParallelPrefixAdder32 : public IOComponent {
public:
    explicit ParallelPrefixAdder32(std::string name);
    const char* getTypeName() const override { return "ParallelPrefixAdder32"; }
    void buildInternals(ComponentBuilder& b) override;
};
