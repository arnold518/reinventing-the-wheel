#pragma once

#include "components/IOComponent.hpp"

class SRLatch : public IOComponent {
public:
    SRLatch(std::string name);
    static constexpr const char* TypeName = "SRLatch";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class GatedDLatch : public IOComponent {
public:
    GatedDLatch(std::string name);
    static constexpr const char* TypeName = "GatedDLatch";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
