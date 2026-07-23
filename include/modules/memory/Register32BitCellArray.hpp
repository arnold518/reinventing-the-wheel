#pragma once

#include "components/IOComponent.hpp"

class Register32BitCellArray : public IOComponent {
public:
    Register32BitCellArray(std::string name);
    static constexpr const char* TypeName = "Register32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
