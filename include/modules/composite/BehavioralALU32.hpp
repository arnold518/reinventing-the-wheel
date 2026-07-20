#pragma once

#include "components/BasicComponent.hpp"

class BehavioralALU32 : public BasicComponent {
public:
    explicit BehavioralALU32(std::string name);
    static constexpr const char* TypeName = "BehavioralALU32";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
