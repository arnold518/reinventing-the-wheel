#pragma once
#include "components/BasicComponent.hpp"
#include <iostream>

class ANDGate : public BasicComponent
{
public:
    ANDGate(std::string name);
    static constexpr const char* TypeName = "ANDGate";
    const char* getTypeName() const override { return TypeName; }
    void initPins(std::shared_ptr<IOComponent> self_ptr) override;
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class NANDGate : public BasicComponent
{
public:
    NANDGate(std::string name);
    static constexpr const char* TypeName = "NANDGate";
    const char* getTypeName() const override { return TypeName; }
    void initPins(std::shared_ptr<IOComponent> self_ptr) override;
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class ORGate : public BasicComponent
{
public:
    ORGate(std::string name);
    static constexpr const char* TypeName = "ORGate";
    const char* getTypeName() const override { return TypeName; }
    void initPins(std::shared_ptr<IOComponent> self_ptr) override;
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class NORGate : public BasicComponent
{
public:
    NORGate(std::string name);
    static constexpr const char* TypeName = "NORGate";
    const char* getTypeName() const override { return TypeName; }
    void initPins(std::shared_ptr<IOComponent> self_ptr) override;
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class XORGate : public BasicComponent
{
public:
    XORGate(std::string name);
    static constexpr const char* TypeName = "XORGate";
    const char* getTypeName() const override { return TypeName; }
    void initPins(std::shared_ptr<IOComponent> self_ptr) override;
    void evaluate(size_t current_time, Simulator& simulator) override;
};