#pragma once

#include "components/IOComponent.hpp"
#include "ForwardDeclarations.hpp"

class BasicComponent : public IOComponent
{
protected:
    size_t delay;
    void _updateOutputWire(Simulator& simulator, const std::string& pin_name, LogicValue new_value, size_t current_sim_time);

public:
    BasicComponent(std::string name, size_t delay_val, PinInitFunction initializer = nullptr);
    static constexpr const char* TypeName = "BasicComponent";
    const char* getTypeName() const override { return TypeName; }

    size_t getDelay() const;
    virtual void evaluate(size_t current_time, Simulator& simulator) = 0;
};