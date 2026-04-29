#pragma once

#include "components/IOComponent.hpp"

class Adder8 : public IOComponent {
public:
    Adder8(std::string name);
    static constexpr const char* TypeName = "Adder8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
