#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::families {
extern const ComponentFamily Register32;
}

class Register32 : public IOComponent {
public:
    Register32(std::string name);
    static constexpr const char* TypeName = "Register32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
