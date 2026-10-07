#pragma once
// Private factory implementation; callers use families::RegisterFile32x32.

#include "components/BasicComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include <array>
#include <cstddef>
#include <utility>
#include <vector>

class RegisterFile32x32Direct : public BasicComponent,
                                             public RegisterStateView {
public:
    RegisterFile32x32Direct(std::string name);
    static constexpr const char* TypeName = "RegisterFile32x32";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;
    std::vector<std::vector<LogicValue>> getRegisterStateAtTime(
        size_t target_time) const override;

private:
    using RegisterSnapshot = std::array<std::vector<LogicValue>, 32>;

    void recordRegisterHistory(size_t time);

    RegisterSnapshot registers;
    std::vector<std::pair<size_t, RegisterSnapshot>> register_history;
    LogicValue previous_clk;
};
