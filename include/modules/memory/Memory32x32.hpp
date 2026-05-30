#pragma once

#include "components/IOComponent.hpp"

class Memory32x32 : public IOComponent {
public:
    Memory32x32(std::string name);
    static constexpr const char* TypeName = "Memory32x32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
