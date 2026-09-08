#pragma once
#include "modules/composite/ALU8Families.hpp"
#include "components/IOComponent.hpp"

// OUT = A + (SUB ? ~B : B) + C_IN, modulo 256.
// CARRY_OUT means no borrow for subtraction; ordinary SUB requires C_IN=1.
class AddSub8 : public IOComponent {
public:
    explicit AddSub8(std::string name);
    static constexpr const char* TypeName = "AddSub8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
