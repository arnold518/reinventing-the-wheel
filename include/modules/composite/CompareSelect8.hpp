#pragma once
#include "modules/composite/ALU8Families.hpp"
#include "components/IOComponent.hpp"

// Consumes DIFF/CARRY_IN/OVERFLOW_IN from A-B in the shared arithmetic unit.
// OP: EQ, SLTU, SLT, MINU, MAXU, MINS, MAXS, zero (0..7).
class CompareSelect8 : public IOComponent {
public:
    explicit CompareSelect8(std::string name);
    static constexpr const char* TypeName = "CompareSelect8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
