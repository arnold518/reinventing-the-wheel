#pragma once

#include "components/IOComponent.hpp"

class AND8 : public IOComponent {
public:
    AND8(std::string name);
    static constexpr const char* TypeName = "AND8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class OR8 : public IOComponent {
public:
    OR8(std::string name);
    static constexpr const char* TypeName = "OR8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

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

class NAND8 : public IOComponent {
public:
    NAND8(std::string name);
    static constexpr const char* TypeName = "NAND8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class NOR8 : public IOComponent {
public:
    NOR8(std::string name);
    static constexpr const char* TypeName = "NOR8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
