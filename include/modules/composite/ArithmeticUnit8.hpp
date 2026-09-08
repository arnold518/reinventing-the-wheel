#pragma once
#include "modules/composite/ALU8Families.hpp"
#include "components/IOComponent.hpp"

// Local OP: ADD, ADC, SUB, SBC, INC, DEC, NEG, reserved (0..7).
class ArithmeticUnit8 : public IOComponent {
public:
    explicit ArithmeticUnit8(std::string name);
    static constexpr const char* TypeName = "ArithmeticUnit8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
