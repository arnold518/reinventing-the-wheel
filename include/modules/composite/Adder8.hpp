#pragma once

#include "components/BasicComponent.hpp"

class Adder8 : public BasicComponent {
public:
    Adder8(std::string name);
    static constexpr const char* TypeName = "Adder8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
