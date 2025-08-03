#pragma once
#include "components/IOComponent.hpp"
#include "ForwardDeclarations.hpp"

class BasicComponent : public IOComponent
{
public:
    BasicComponent(std::string name, size_t delay_val = 1);
    static constexpr const char* TypeName = "BasicComponent";
    const char* getTypeName() const override { return TypeName; }

    void initPins(std::shared_ptr<IOComponent> self_ptr) override = 0;

    virtual void evaluate(size_t current_time, Simulator& simulator) = 0;
};