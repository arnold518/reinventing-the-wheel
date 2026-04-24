#pragma once

#include "components/BasicComponent.hpp"

class TwosComplement8 : public BasicComponent {
public:
    TwosComplement8(std::string name);
    static constexpr const char* TypeName = "TwosComplement8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class Subtractor8 : public BasicComponent {
public:
    Subtractor8(std::string name);
    static constexpr const char* TypeName = "Subtractor8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class SubtractorWithBorrow8 : public BasicComponent {
public:
    SubtractorWithBorrow8(std::string name);
    static constexpr const char* TypeName = "SubtractorWithBorrow8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class Incrementer8 : public BasicComponent {
public:
    Incrementer8(std::string name);
    static constexpr const char* TypeName = "Incrementer8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class Decrementer8 : public BasicComponent {
public:
    Decrementer8(std::string name);
    static constexpr const char* TypeName = "Decrementer8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
