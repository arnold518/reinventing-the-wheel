#pragma once

#include "simulator/SimulationTime.hpp"

#include "components/BasicComponent.hpp"

// The register's rule for one evaluation, shared by every compact part that
// holds a register: unknown reset, enable or data leave X exactly where the
// gate-level register does.
void registerStep(std::vector<LogicValue>& stored, LogicValue& previous_clock, LogicValue reset, LogicValue clock,
                  LogicValue enable, const std::vector<LogicValue>& data);

// Compact implementation of the same width-parametric register contract.
class RegisterBehavioral : public BasicComponent {
public:
    RegisterBehavioral(std::string name, size_t width);
    const char* getTypeName() const override { return type_.c_str(); }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
    StateSupport stateSupport() const override { return StateSupport::Saved; }
    void saveOwnState(circuit::state::Writer& out) const override;
    void restoreOwnState(circuit::state::Reader& in) override;

private:
    size_t width_;
    std::string type_;
    std::vector<LogicValue> stored_;
    LogicValue previous_clock_ = LogicValue::UNKNOWN;
};
