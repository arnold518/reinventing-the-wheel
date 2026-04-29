#pragma once

#include "components/IOComponent.hpp"

class FullAdder : public IOComponent
{
public:
    static constexpr const char* TypeName = "FullAdder";
    const char* getTypeName() const override { return TypeName; }

    FullAdder(std::string name);
    void buildInternals(ComponentBuilder& builder) override;
};
