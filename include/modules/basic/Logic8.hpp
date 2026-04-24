#pragma once

#include "components/BasicComponent.hpp"

class AND8 : public BasicComponent {
public:
    AND8(std::string name);
    static constexpr const char* TypeName = "AND8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class OR8 : public BasicComponent {
public:
    OR8(std::string name);
    static constexpr const char* TypeName = "OR8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class XOR8 : public BasicComponent {
public:
    XOR8(std::string name);
    static constexpr const char* TypeName = "XOR8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class NOT8 : public BasicComponent {
public:
    NOT8(std::string name);
    static constexpr const char* TypeName = "NOT8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class NAND8 : public BasicComponent {
public:
    NAND8(std::string name);
    static constexpr const char* TypeName = "NAND8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class NOR8 : public BasicComponent {
public:
    NOR8(std::string name);
    static constexpr const char* TypeName = "NOR8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
