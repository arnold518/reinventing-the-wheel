#pragma once

#include "components/IOComponent.hpp"

class RegisterFile32x32 : public IOComponent {
public:
    RegisterFile32x32(std::string name);
    static constexpr const char* TypeName = "RegisterFile32x32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
