#pragma once

#include "simulator/SimulationTime.hpp"

#include "components/BasicComponent.hpp"
#include <utility>

namespace circuit::detail {
// The ordered pin tables remain the public interface. Evaluation resolves
// fixed gate inputs once. Pin-table edits invalidate handles immediately.
class UnaryGateInputs {
    bool resolved_ = false;
    std::shared_ptr<Pin<1>> input_;
public:
    void invalidate() noexcept { input_.reset(); resolved_ = false; }
    LogicValue read(const IOComponent& owner) {
        if (!resolved_) {
            input_ = owner.getInputPin<1>("IN");
            resolved_ = true;
        }
        return input_ ? input_->packed().get(0) : LogicValue::UNKNOWN;
    }
};

class BinaryGateInputs {
    bool resolved_ = false;
    std::shared_ptr<Pin<1>> a_, b_;
public:
    void invalidate() noexcept { a_.reset(); b_.reset(); resolved_ = false; }
    std::pair<LogicValue, LogicValue> read(const IOComponent& owner) {
        if (!resolved_) {
            a_ = owner.getInputPin<1>("A");
            b_ = owner.getInputPin<1>("B");
            resolved_ = true;
        }
        return {a_ ? a_->packed().get(0) : LogicValue::UNKNOWN,
                b_ ? b_->packed().get(0) : LogicValue::UNKNOWN};
    }
};
}

class NOTGate : public BasicComponent
{
    circuit::detail::UnaryGateInputs inputs_;
public:
    NOTGate(std::string name);
    static constexpr const char* TypeName = "NOTGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    void pinsChanged() noexcept override { inputs_.invalidate(); }
    bool inertialDelay() const override { return true; }
};

class ANDGate : public BasicComponent
{
    circuit::detail::BinaryGateInputs inputs_;
public:
    ANDGate(std::string name);
    static constexpr const char* TypeName = "ANDGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    void pinsChanged() noexcept override { inputs_.invalidate(); }
    bool inertialDelay() const override { return true; }
};

class NANDGate : public BasicComponent
{
    circuit::detail::BinaryGateInputs inputs_;
public:
    NANDGate(std::string name);
    static constexpr const char* TypeName = "NANDGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    void pinsChanged() noexcept override { inputs_.invalidate(); }
    bool inertialDelay() const override { return true; }
};

class ORGate : public BasicComponent
{
    circuit::detail::BinaryGateInputs inputs_;
public:
    ORGate(std::string name);
    static constexpr const char* TypeName = "ORGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    void pinsChanged() noexcept override { inputs_.invalidate(); }
    bool inertialDelay() const override { return true; }
};

class NORGate : public BasicComponent
{
    circuit::detail::BinaryGateInputs inputs_;
public:
    NORGate(std::string name);
    static constexpr const char* TypeName = "NORGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    void pinsChanged() noexcept override { inputs_.invalidate(); }
    bool inertialDelay() const override { return true; }
};

class XORGate : public BasicComponent
{
    circuit::detail::BinaryGateInputs inputs_;
public:
    XORGate(std::string name);
    static constexpr const char* TypeName = "XORGate";
    const char* getTypeName() const override { return TypeName; }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
protected:
    void pinsChanged() noexcept override { inputs_.invalidate(); }
    bool inertialDelay() const override { return true; }
};
