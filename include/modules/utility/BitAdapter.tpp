#pragma once

#include "modules/utility/BitAdapter.hpp"
#include "simulator/Event.hpp"
#include <memory>
#include <utility>
#include <vector>

template<size_t WIDTH>
BitSplitter<WIDTH>::BitSplitter(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
          self->addPin<WIDTH>("IN", PinType::INPUT);
          for (size_t i = 0; i < WIDTH; ++i) {
              self->addPin("OUT_" + std::to_string(i), PinType::OUTPUT);
          }
      }) {}

template<size_t WIDTH>
void BitSplitter<WIDTH>::evaluate(size_t current_time, Simulator& simulator) {
    auto input = getInputPin<WIDTH>("IN");
    if (!input) {
        return;
    }

    for (size_t i = 0; i < WIDTH; ++i) {
        _updateOutputWire(simulator, "OUT_" + std::to_string(i), input->getBit(i), current_time);
    }
}

template<size_t WIDTH>
BitJoiner<WIDTH>::BitJoiner(std::string name)
    : BasicComponent(std::move(name), 0, [](IOComponent* self) {
          for (size_t i = 0; i < WIDTH; ++i) {
              self->addPin("IN_" + std::to_string(i), PinType::INPUT);
          }
          self->addPin<WIDTH>("OUT", PinType::OUTPUT);
      }) {}

template<size_t WIDTH>
void BitJoiner<WIDTH>::evaluate(size_t current_time, Simulator& simulator) {
    std::vector<LogicValue> values(WIDTH, LogicValue::UNKNOWN);
    for (size_t i = 0; i < WIDTH; ++i) {
        if (auto input = getInputPin("IN_" + std::to_string(i))) {
            values[i] = input->getValue();
        }
    }

    auto output = getOutputPin<WIDTH>("OUT");
    if (!output) {
        return;
    }

    output->setValueFromVector(values);
    if (auto wire = output->getExternalWire()) {
        simulator.scheduleEvent(std::make_shared<WireUpdateEvent<WIDTH>>(current_time + getDelay(), wire, values));
    } else {
        simulator.recordPinChange(current_time + getDelay(), output, values);
    }
}
