#pragma once

#include "components/IOComponent.hpp"

class Decoder2to4 : public IOComponent {
public:
    Decoder2to4(std::string name);
    static constexpr const char* TypeName = "Decoder2to4";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Decoder5to32 : public IOComponent {
public:
    Decoder5to32(std::string name);
    static constexpr const char* TypeName = "Decoder5to32";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
