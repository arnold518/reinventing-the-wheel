#pragma once

#include "components/IOComponent.hpp"
#include "basic/Wire.hpp"
#include "ForwardDeclarations.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"

class BasicComponent : public IOComponent
{
protected:
    size_t delay;
    void _updateOutputWire(Simulator& simulator, const std::string& pin_name, LogicValue new_value, size_t current_sim_time);

    template<size_t WIDTH>
    void _updateOutputWire(Simulator& simulator, const std::string& pin_name, uint64_t new_value, size_t current_sim_time) {
        if (auto pin = getOutputPin<WIDTH>(pin_name)) {
            pin->setValueFromUInt64(new_value);
            if (auto wire = pin->getExternalWire()) {
                simulator.scheduleEvent(std::make_shared<WireUpdateEvent<WIDTH>>(
                    current_sim_time + delay, wire, new_value));
            } else {
                simulator.recordPinChange(current_sim_time + delay, pin, pin->getValueAsVector());
            }
        }
    }

public:
    BasicComponent(std::string name, size_t delay_val, PinInitFunction initializer = nullptr);
    static constexpr const char* TypeName = "BasicComponent";
    const char* getTypeName() const override { return TypeName; }

    size_t getDelay() const;
    virtual void scheduleInitialEvents(Simulator& simulator, size_t current_time);
    virtual void evaluate(size_t current_time, Simulator& simulator) = 0;
};
