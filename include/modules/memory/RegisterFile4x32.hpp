#pragma once

#include "components/IOComponent.hpp"

class RegisterFile4x32 : public IOComponent {
public:
    RegisterFile4x32(std::string name);
    static constexpr const char* TypeName = "RegisterFile4x32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
