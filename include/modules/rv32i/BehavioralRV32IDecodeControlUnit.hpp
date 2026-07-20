#pragma once

#include "components/BasicComponent.hpp"

class BehavioralRV32IDecodeControlUnit : public BasicComponent {
public:
    explicit BehavioralRV32IDecodeControlUnit(std::string name);
    static constexpr const char* TypeName = "BehavioralRV32IDecodeControlUnit";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
