#pragma once

#include "components/BasicComponent.hpp"

class HalfAdder : public BasicComponent
{
public:
    static constexpr const char* TypeName = "HalfAdder";
    const char* getTypeName() const override { return TypeName; }

    HalfAdder(std::string name);
    void evaluate(size_t current_time, Simulator& simulator) override;
};
