#include "modules/memory/BehavioralMemoryBit.hpp"

#include <utility>

BehavioralMemoryBit::BehavioralMemoryBit(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
          self->addPin("D", PinType::INPUT);
          self->addPin("WE", PinType::INPUT);
          self->addPin("CLK", PinType::INPUT);
          self->addPin("RST", PinType::INPUT);
          self->addPin("Q", PinType::OUTPUT);
      }),
      stored_value(LogicValue::UNKNOWN),
      previous_clk(LogicValue::UNKNOWN) {}

void BehavioralMemoryBit::evaluate(size_t current_time, Simulator& simulator) {
    const auto d = getInputValue("D");
    const auto we = getInputValue("WE");
    const auto clk = getInputValue("CLK");
    const auto rst = getInputValue("RST");

    if (rst == LogicValue::HIGH) {
        stored_value = LogicValue::LOW;
    } else if (rst != LogicValue::LOW) {
        stored_value = LogicValue::UNKNOWN;
    } else if (previous_clk == LogicValue::LOW && clk == LogicValue::HIGH) {
        if (we == LogicValue::HIGH) {
            stored_value = (d == LogicValue::HIGH || d == LogicValue::LOW) ? d : LogicValue::UNKNOWN;
        } else if (we != LogicValue::LOW) {
            stored_value = LogicValue::UNKNOWN;
        }
    }

    previous_clk = clk;
    _updateOutputWire(simulator, "Q", stored_value, current_time);
}
