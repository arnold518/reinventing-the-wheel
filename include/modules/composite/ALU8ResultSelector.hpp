#pragma once
#include "modules/composite/ALU8Families.hpp"
#include "components/IOComponent.hpp"

// Structural ALU integration block; see buildInternals for the gate network.
class ALU8ResultSelector : public IOComponent {
public:
    explicit ALU8ResultSelector(std::string name);
    static constexpr const char* TypeName = "ALU8ResultSelector";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
