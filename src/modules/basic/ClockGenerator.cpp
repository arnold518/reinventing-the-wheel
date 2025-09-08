#include "modules/basic/ClockGenerator.hpp"
#include "simulator/Simulator.hpp"
#include "simulator/Event.hpp"

ClockGenerator::ClockGenerator(std::string name, size_t half_period) 
    : BasicComponent(std::move(name), 0, 
      [](IOComponent* self) {
          self->addPin("CLK_OUT", PinType::OUTPUT);
      }), 
      current_state(LogicValue::LOW), 
      period_half(half_period)
{}

void ClockGenerator::evaluate(size_t current_time, Simulator& simulator) {
    current_state = (current_state == LogicValue::HIGH) ? LogicValue::LOW : LogicValue::HIGH;
    _updateOutputWire(simulator, "CLK_OUT", current_state, current_time);

    simulator.scheduleEvent(std::make_shared<ComponentEvalEvent>(current_time + period_half, shared_from_this()));
}

void ClockGenerator::startClock(Simulator& simulator, size_t start_time) {
    simulator.scheduleEvent(std::make_shared<ComponentEvalEvent>(start_time, shared_from_this()));
}