#pragma once

#include "components/IOComponent.hpp"

class XOR8 : public IOComponent {
public:
    XOR8(std::string name);
    static constexpr const char* TypeName = "XOR8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class NOT8 : public IOComponent {
public:
    NOT8(std::string name);
    static constexpr const char* TypeName = "NOT8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
