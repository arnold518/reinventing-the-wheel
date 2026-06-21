#include "components/BasicComponent.hpp"
#include "simulator/Simulator.hpp"
#include "simulator/Event.hpp"
#include "basic/Wire.hpp"
#include "basic/Pin.hpp"

BasicComponent::BasicComponent(std::string name, size_t delay_val, PinInitFunction initializer)
    : IOComponent(std::move(name), std::move(initializer)),
      delay(delay_val) {}

size_t BasicComponent::getDelay() const {
    return delay;
}

void BasicComponent::scheduleInitialEvents(Simulator& simulator, size_t current_time) {
    (void)simulator;
    (void)current_time;
}

void BasicComponent::_updateOutputWire(Simulator& simulator, const std::string& pin_name, LogicValue new_value, size_t current_sim_time) {
    if (auto pin = getOutputPin(pin_name)) {
        pin->setBit(0, new_value);
        if (auto wire = pin->getExternalWire()) {
            simulator.scheduleEvent(std::make_shared<WireUpdateEvent<>>(current_sim_time + this->delay, wire, new_value));
        } else {
            simulator.recordPinChange(current_sim_time + this->delay, pin, std::vector<LogicValue>{new_value});
        }
    }
}
