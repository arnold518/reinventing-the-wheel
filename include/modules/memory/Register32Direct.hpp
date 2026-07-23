#pragma once

#include "components/BasicComponent.hpp"
#include <string>
#include <vector>

class Register32Direct : public BasicComponent {
public:
    explicit Register32Direct(std::string name);
    static constexpr const char* TypeName = "Register32";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    std::vector<LogicValue> stored_value_;
    LogicValue previous_clk_ = LogicValue::UNKNOWN;
};
