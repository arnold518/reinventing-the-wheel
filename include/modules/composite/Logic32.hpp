#pragma once

#include "components/IOComponent.hpp"

class Logic32 : public IOComponent {
public:
    explicit Logic32(std::string name);
    static constexpr const char* TypeName = "Logic32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
