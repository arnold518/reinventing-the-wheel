#pragma once

#include "components/IOComponent.hpp"

class HalfAdder : public IOComponent
{
public:
    static constexpr const char* TypeName = "HalfAdder";
    const char* getTypeName() const override { return TypeName; }

    HalfAdder(std::string name);
    void buildInternals(ComponentBuilder& builder) override;
};
