#pragma once

#include "components/IOComponent.hpp"

class FullAdder : public IOComponent
{
public:
    static constexpr const char* TypeName = "FullAdder";
    const char* getTypeName() const override { return TypeName; }

    // The constructor will define the external pins.
    FullAdder(std::string name);

    // This method will build the internal components.
    void buildInternals(ComponentBuilder& builder) override;
};