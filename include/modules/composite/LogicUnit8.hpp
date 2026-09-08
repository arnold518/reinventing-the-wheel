#pragma once
#include "modules/composite/ALU8Families.hpp"
#include "components/IOComponent.hpp"

// Local OP: AND, OR, XOR, NOT A, NAND, NOR, XNOR, zero (0..7).
class LogicUnit8 : public IOComponent {
public:
    explicit LogicUnit8(std::string name);
    static constexpr const char* TypeName = "LogicUnit8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
