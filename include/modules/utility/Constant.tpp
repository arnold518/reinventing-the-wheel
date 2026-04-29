#pragma once

#include "modules/utility/Constant.hpp"
#include <utility>

template<size_t OUT_WIDTH, size_t TRIGGER_WIDTH>
ConstantValue<OUT_WIDTH, TRIGGER_WIDTH>::ConstantValue(std::string name, uint64_t constant_value)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
          self->addPin<TRIGGER_WIDTH>("TRIGGER", PinType::INPUT);
          self->addPin<OUT_WIDTH>("OUT", PinType::OUTPUT);
      }),
      value(constant_value) {}

template<size_t OUT_WIDTH, size_t TRIGGER_WIDTH>
void ConstantValue<OUT_WIDTH, TRIGGER_WIDTH>::evaluate(size_t current_time, Simulator& simulator) {
    if constexpr (OUT_WIDTH == 1) {
        _updateOutputWire(simulator, "OUT", (value & 1U) ? LogicValue::HIGH : LogicValue::LOW, current_time);
    } else {
        _updateOutputWire<OUT_WIDTH>(simulator, "OUT", value, current_time);
    }
}
