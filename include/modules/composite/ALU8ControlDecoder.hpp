#pragma once
#include "modules/composite/ALU8Families.hpp"
#include "components/IOComponent.hpp"

// Structural ALU integration block; see buildInternals for the gate network.
class ALU8ControlDecoder : public IOComponent {
public:
    explicit ALU8ControlDecoder(std::string name);
    static constexpr const char* TypeName = "ALU8ControlDecoder";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
