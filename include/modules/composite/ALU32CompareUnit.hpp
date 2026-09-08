#pragma once
#include "components/IOComponent.hpp"

// Structural ALU integration block; see buildInternals for the gate network.
class ALU32CompareUnit : public IOComponent {
public:
    explicit ALU32CompareUnit(std::string name);
    static constexpr const char* TypeName = "ALU32CompareUnit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
