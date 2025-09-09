#pragma once

#include "components/IOComponent.hpp"

class HalfAdder : public IOComponent
{
public:
    static constexpr const char* TypeName = "HalfAdder";
    const char* getTypeName() const override { return TypeName; }

    // The constructor will define the external pins.
    HalfAdder(std::string name);

    // This method will build the internal XOR and AND gates.
    void buildInternals(ComponentBuilder& builder) override;
};