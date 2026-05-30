#pragma once

#include "components/IOComponent.hpp"

class Memory4x32 : public IOComponent {
public:
    Memory4x32(std::string name);
    static constexpr const char* TypeName = "Memory4x32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
