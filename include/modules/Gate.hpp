#pragma once
#include "components/BasicComponent.hpp"
#include <iostream>

class ANDGate : public BasicComponent
{
public:
    ANDGate(std::string name);
    static constexpr const char* TypeName = "ANDGate";
    void initPins(std::shared_ptr<IOComponent> self_ptr) override;
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class NANDGate : public BasicComponent
{
public:
    NANDGate(std::string name);
    static constexpr const char* TypeName = "NANDGate";
    void initPins(std::shared_ptr<IOComponent> self_ptr) override;
    void evaluate(size_t current_time, Simulator& simulator) override;
};