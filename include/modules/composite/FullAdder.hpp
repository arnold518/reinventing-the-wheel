#pragma once

#include "components/BasicComponent.hpp"

class FullAdder : public BasicComponent
{
public:
    static constexpr const char* TypeName = "FullAdder";
    const char* getTypeName() const override { return TypeName; }

    FullAdder(std::string name);
    void evaluate(size_t current_time, Simulator& simulator) override;
};
