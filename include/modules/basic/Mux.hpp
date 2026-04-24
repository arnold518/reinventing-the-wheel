#pragma once

#include "components/BasicComponent.hpp"

class Mux2to1 : public BasicComponent {
public:
    Mux2to1(std::string name);
    static constexpr const char* TypeName = "Mux2to1";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class Mux4to1 : public BasicComponent {
public:
    Mux4to1(std::string name);
    static constexpr const char* TypeName = "Mux4to1";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class Mux8to1 : public BasicComponent {
public:
    Mux8to1(std::string name);
    static constexpr const char* TypeName = "Mux8to1";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class Mux16to1 : public BasicComponent {
public:
    Mux16to1(std::string name);
    static constexpr const char* TypeName = "Mux16to1";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class Mux2to1_8bit : public BasicComponent {
public:
    Mux2to1_8bit(std::string name);
    static constexpr const char* TypeName = "Mux2to1_8bit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class Mux4to1_8bit : public BasicComponent {
public:
    Mux4to1_8bit(std::string name);
    static constexpr const char* TypeName = "Mux4to1_8bit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class Mux8to1_8bit : public BasicComponent {
public:
    Mux8to1_8bit(std::string name);
    static constexpr const char* TypeName = "Mux8to1_8bit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class Mux16to1_8bit : public BasicComponent {
public:
    Mux16to1_8bit(std::string name);
    static constexpr const char* TypeName = "Mux16to1_8bit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
