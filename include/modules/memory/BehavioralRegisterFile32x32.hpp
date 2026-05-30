#pragma once

#include "components/BasicComponent.hpp"
#include <array>
#include <vector>

class BehavioralRegisterFile32x32 : public BasicComponent {
public:
    BehavioralRegisterFile32x32(std::string name);
    static constexpr const char* TypeName = "BehavioralRegisterFile32x32";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    std::array<std::vector<LogicValue>, 32> registers;
    LogicValue previous_clk;
};
