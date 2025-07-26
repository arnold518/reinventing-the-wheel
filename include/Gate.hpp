#pragma once
#include "BasicComponent.hpp"
#include <iostream>

class ANDGate : public BasicComponent
{
public:
    ANDGate(std::string name);
    void initPins(std::shared_ptr<IOComponent> self_ptr) override;
    void evaluate(size_t current_time, Simulator& simulator) override;
};

class NANDGate : public BasicComponent
{
public:
    NANDGate(std::string name);
    void initPins(std::shared_ptr<IOComponent> self_ptr) override;
    void evaluate(size_t current_time, Simulator& simulator) override;
};