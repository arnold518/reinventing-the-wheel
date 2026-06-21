#pragma once

#include "modules/utility/Constant.hpp"
#include <utility>

template<size_t OUT_WIDTH, size_t LEGACY_WIDTH>
ConstantValue<OUT_WIDTH, LEGACY_WIDTH>::ConstantValue(std::string name, uint64_t constant_value)
    : BasicComponent(std::move(name), 0, [](IOComponent* self) {
          self->addPin<OUT_WIDTH>("OUT", PinType::OUTPUT);
      }),
      value(constant_value) {}

template<size_t OUT_WIDTH, size_t LEGACY_WIDTH>
void ConstantValue<OUT_WIDTH, LEGACY_WIDTH>::scheduleInitialEvents(Simulator& simulator, size_t current_time) {
    evaluate(current_time, simulator);
}

template<size_t OUT_WIDTH, size_t LEGACY_WIDTH>
void ConstantValue<OUT_WIDTH, LEGACY_WIDTH>::evaluate(size_t current_time, Simulator& simulator) {
    if constexpr (OUT_WIDTH == 1) {
        _updateOutputWire(simulator, "OUT", (value & 1U) ? LogicValue::HIGH : LogicValue::LOW, current_time);
    } else {
        _updateOutputWire<OUT_WIDTH>(simulator, "OUT", value, current_time);
    }
}
