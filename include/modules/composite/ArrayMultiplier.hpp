#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <string>

namespace circuit { class ComponentCatalog; }

// One-step W × W multiplier (§ 6.1.1): AND-gate partial products, a Dadda
// tree of full and half adders and prefix adders. Combinational: LO and HI
// settle from A, B, SIGNED_A and SIGNED_B with no clock. SIGNED_A/SIGNED_B
// read that operand as two's complement, as the RV32IM MULH/MULHSU/MULHU
// selections do for the iterative Multiplier<W>.
class ArrayMultiplier : public IOComponent {
public:
    ArrayMultiplier(std::string name, size_t width);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
private:
    size_t width_;
    std::string type_;
};

// MulDivUnit<W> with the array multiplier (§ 6.1.2): the same pins and
// handshake; a multiply is DONE two edges after START (launch, product),
// a divide stays iterative (W + 1 edges).
class ArrayMulDivUnit : public IOComponent {
public:
    ArrayMulDivUnit(std::string name, size_t width);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
private:
    size_t width_;
    std::string type_;
};

namespace circuit::families {
// arithmetic.array-multiplier.W for W = 4, 8, 16, 32.
const ComponentFamily& arrayMultiplier(size_t width);
// arithmetic.array-mul-div.W for W = 4, 8, 16, 32.
const ComponentFamily& arrayMulDivUnit(size_t width);
}
namespace circuit::arithmetic {
void registerArrayMultipliers(ComponentCatalog&);
}
