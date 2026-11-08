#pragma once

#include "simulator/SimulationTime.hpp"

#include "components/BasicComponent.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

template<size_t OUT_WIDTH>
class ConstantValue : public BasicComponent {
public:
    ConstantValue(std::string name, uint64_t constant_value);
    static constexpr const char* TypeName = "ConstantValue";
    const char* getTypeName() const override { return TypeName; }
    uint64_t getConstantValue() const { return value; }
    void scheduleInitialEvents(Simulator& simulator, SimulationTime current_time) override;
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
    StateSupport stateSupport() const override { return StateSupport::Stateless; }

private:
    uint64_t value;
};

#include "modules/utility/Constant.tpp"
