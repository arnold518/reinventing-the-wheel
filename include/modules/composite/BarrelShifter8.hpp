#pragma once
#include "modules/composite/ALU8Families.hpp"
#include "components/IOComponent.hpp"

// OP: SLL, SRL, SRA, ROL, ROR (0..4); 5..7 produce zero data/carry.
// Distance is B[2:0]; zero distance forwards A and C_IN.
class BarrelShifter8 : public IOComponent {
public:
    explicit BarrelShifter8(std::string name);
    static constexpr const char* TypeName = "BarrelShifter8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
