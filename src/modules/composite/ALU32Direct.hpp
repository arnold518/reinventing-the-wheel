#pragma once
// Private factory implementation; callers use families::ALU32.

#include "components/BasicComponent.hpp"

class ALU32Direct : public BasicComponent {
public:
    explicit ALU32Direct(std::string name);
    static constexpr const char* TypeName = "ALU32";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
};
