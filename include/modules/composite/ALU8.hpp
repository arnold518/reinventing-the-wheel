#pragma once

#include "components/BasicComponent.hpp"

class ALU8 : public BasicComponent {
public:
    ALU8(std::string name);
    static constexpr const char* TypeName = "ALU8";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
