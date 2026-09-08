#pragma once
#include "modules/composite/ALU8Families.hpp"
#include "components/IOComponent.hpp"

// Derives ZERO/NEGATIVE from RESULT; selects arithmetic or shift flags.
class ALU8FlagUnit : public IOComponent {
public:
    explicit ALU8FlagUnit(std::string name);
    static constexpr const char* TypeName = "ALU8FlagUnit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
