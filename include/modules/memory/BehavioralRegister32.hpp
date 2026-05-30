#pragma once

#include "components/IOComponent.hpp"

class BehavioralRegister32 : public IOComponent {
public:
    BehavioralRegister32(std::string name);
    static constexpr const char* TypeName = "BehavioralRegister32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
