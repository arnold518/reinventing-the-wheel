#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

// OP[5] -> LOCAL_OP, ARITH_OP, COMPARE_OP, six group enables and INVALID_OP.
// Decoding is independent of the operand width.
class ALUControlDecoder : public IOComponent {
public:
    explicit ALUControlDecoder(std::string name);
    static constexpr const char* TypeName = "ALUControlDecoder";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

namespace circuit::families {
const ComponentFamily& aluControlDecoder();
}
