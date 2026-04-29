#pragma once

#include "components/IOComponent.hpp"

class ALU8 : public IOComponent {
public:
    ALU8(std::string name);
    static constexpr const char* TypeName = "ALU8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
