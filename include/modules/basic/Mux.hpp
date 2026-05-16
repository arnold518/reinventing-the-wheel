#pragma once

#include "components/IOComponent.hpp"

class Mux2to1 : public IOComponent {
public:
    Mux2to1(std::string name);
    static constexpr const char* TypeName = "Mux2to1";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Mux4to1 : public IOComponent {
public:
    Mux4to1(std::string name);
    static constexpr const char* TypeName = "Mux4to1";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Mux8to1 : public IOComponent {
public:
    Mux8to1(std::string name);
    static constexpr const char* TypeName = "Mux8to1";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Mux16to1 : public IOComponent {
public:
    Mux16to1(std::string name);
    static constexpr const char* TypeName = "Mux16to1";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Mux32to1 : public IOComponent {
public:
    Mux32to1(std::string name);
    static constexpr const char* TypeName = "Mux32to1";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Mux2to1_8bit : public IOComponent {
public:
    Mux2to1_8bit(std::string name);
    static constexpr const char* TypeName = "Mux2to1_8bit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Mux4to1_8bit : public IOComponent {
public:
    Mux4to1_8bit(std::string name);
    static constexpr const char* TypeName = "Mux4to1_8bit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Mux8to1_8bit : public IOComponent {
public:
    Mux8to1_8bit(std::string name);
    static constexpr const char* TypeName = "Mux8to1_8bit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Mux16to1_8bit : public IOComponent {
public:
    Mux16to1_8bit(std::string name);
    static constexpr const char* TypeName = "Mux16to1_8bit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class Mux32to1_32bit : public IOComponent {
public:
    Mux32to1_32bit(std::string name);
    static constexpr const char* TypeName = "Mux32to1_32bit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
