#pragma once

#include "components/BasicComponent.hpp"

// Compact implementation of the same width-parametric register contract.
class RegisterDirect : public BasicComponent {
public:
    RegisterDirect(std::string name, size_t width);
    const char* getTypeName() const override { return type_.c_str(); }
    void evaluate(size_t current_time, Simulator& simulator) override;

private:
    size_t width_;
    std::string type_;
    std::vector<LogicValue> stored_;
    LogicValue previous_clock_ = LogicValue::UNKNOWN;
};
