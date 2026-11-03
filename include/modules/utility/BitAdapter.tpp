#pragma once

#include "modules/utility/BitAdapter.hpp"
#include "simulator/Event.hpp"
#include "components/TimingProfile.hpp"
#include <array>
#include <memory>
#include <utility>
#include <vector>

template<size_t WIDTH>
BitSplitter<WIDTH>::BitSplitter(std::string name)
    : BasicComponent(std::move(name), circuit::timing::leafDelay(circuit::timing::Leaf::Splitter), [](IOComponent* self) {
          self->addPin<WIDTH>("IN", PinType::INPUT);
          for (size_t i = 0; i < WIDTH; ++i) {
              self->addPin("OUT_" + std::to_string(i), PinType::OUTPUT);
          }
      }) {}

template<size_t WIDTH>
void BitSplitter<WIDTH>::evaluate(SimulationTime current_time, Simulator& simulator) {
    auto input = getInputPin<WIDTH>("IN");
    if (!input) {
        return;
    }

    // Output names are built once per width, not once per bit and evaluation.
    static const auto names = [] {
        std::array<std::string, WIDTH> result;
        for (size_t i = 0; i < WIDTH; ++i) result[i] = "OUT_" + std::to_string(i);
        return result;
    }();
    for (size_t i = 0; i < WIDTH; ++i) {
        _updateOutputWire(simulator, names[i], input->getBit(i), current_time);
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
void BitJoiner<WIDTH>::evaluate(SimulationTime current_time, Simulator& simulator) {
    if (!resolved_) {
        for (size_t i = 0; i < WIDTH; ++i) inputs_[i] = getInputPin("IN_" + std::to_string(i));
        resolved_ = true;
    }
    PackedLogic value = PackedLogic::allUnknown(WIDTH);
    for (size_t i = 0; i < WIDTH; ++i) {
        if (inputs_[i]) {
            value.set(i, inputs_[i]->packed().get(0));
        }
    }

    auto output = getOutputPin<WIDTH>("OUT");
    if (!output) {
        return;
    }

    output->setPacked(value);
    if (auto wire = output->getExternalWire()) {
        simulator.scheduleEvent(std::make_shared<WireUpdateEvent>(checkedTimeAdd(current_time, getDelay()), wire, value));
    } else {
        simulator.recordPinChange(checkedTimeAdd(current_time, getDelay()), output, value.toVector(WIDTH));
    }
}
