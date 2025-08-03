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