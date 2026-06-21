#pragma once

#include "components/BasicComponent.hpp"
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

class BehavioralRegisterFile32x32 : public BasicComponent {
public:
    BehavioralRegisterFile32x32(std::string name);
    static constexpr const char* TypeName = "BehavioralRegisterFile32x32";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
    std::vector<std::vector<LogicValue>> getRegisterStateAtTime(size_t target_time) const;

private:
    using RegisterSnapshot = std::array<std::vector<LogicValue>, 32>;

    void recordRegisterHistory(size_t time);

    RegisterSnapshot registers;
    std::vector<std::pair<size_t, RegisterSnapshot>> register_history;
    LogicValue previous_clk;
};
