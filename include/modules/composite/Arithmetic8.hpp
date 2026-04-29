#pragma once

#include "components/IOComponent.hpp"

class TwosComplement8 : public IOComponent {
public:
    TwosComplement8(std::string name);
    static constexpr const char* TypeName = "TwosComplement8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Subtractor8 : public IOComponent {
public:
    Subtractor8(std::string name);
    static constexpr const char* TypeName = "Subtractor8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class SubtractorWithBorrow8 : public IOComponent {
public:
    SubtractorWithBorrow8(std::string name);
    static constexpr const char* TypeName = "SubtractorWithBorrow8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Incrementer8 : public IOComponent {
public:
    Incrementer8(std::string name);
    static constexpr const char* TypeName = "Incrementer8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Decrementer8 : public IOComponent {
public:
    Decrementer8(std::string name);
    static constexpr const char* TypeName = "Decrementer8";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
