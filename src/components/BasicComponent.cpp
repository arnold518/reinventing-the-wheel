#include "components/BasicComponent.hpp"
#include <stdexcept>

BasicComponent::BasicComponent(std::string name, size_t delay_val, PinInitFunction initializer)
    : IOComponent(std::move(name), std::move(initializer)), delay(delay_val) {}

size_t BasicComponent::getDelay() const {
    return delay;
}

void BasicComponent::scheduleInitialEvents(Simulator& simulator,
                                           size_t current_time) {
    // Most evaluated components are triggered by their input-wire updates.
    // Self-starting components such as ConstantValue override this hook, while
    // ClockGenerator is started explicitly so it cannot be scheduled twice.
    (void)simulator;
    (void)current_time;
}

void BasicComponent::_updateOutputWire(Simulator& simulator,
                                       const std::string& pin_name,
    LogicValue new_value,
    size_t current_sim_time) {
    if (auto pin = getOutputPin(pin_name)) {
        pin->setBit(0, new_value);
        if (auto wire = pin->getExternalWire()) {
            simulator.scheduleEvent(std::make_shared<WireUpdateEvent<>>(
                current_sim_time + delay, wire, new_value));
        } else {
            simulator.recordPinChange(
                current_sim_time + delay, pin, pin->getValueAsVector());
        }
    }
}

void BasicComponent::_updateOutputWireDynamic(Simulator& simulator,
                                              const std::string& pin_name,
    const std::vector<LogicValue>& new_value,
    size_t current_sim_time) {
    const auto pin = getOutputPinDynamic(pin_name);
    if (!pin || pin->getWidth() != new_value.size()) {
        return;
    }
    pin->setValueFromVector(new_value);
    if (auto wire = pin->getExternalWireBase()) {
        auto event = makeWireUpdateEvent(current_sim_time + delay, wire, new_value);
        if (!event) {
            throw std::logic_error("Unsupported output pin width");
        }
        simulator.scheduleEvent(std::move(event));
    } else {
        simulator.recordPinChange(
            current_sim_time + delay, pin, pin->getValueAsVector());
    }
}
