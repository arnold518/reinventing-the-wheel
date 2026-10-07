#include "modules/memory/RegisterDirect.hpp"
#include "modules/memory/Register.hpp"

RegisterDirect::RegisterDirect(std::string name, size_t width)
    : BasicComponent(std::move(name), 1, [width](IOComponent* self) {
          self->addPinDynamic("D", PinType::INPUT, width);
          self->addPinDynamic("Q", PinType::OUTPUT, width);
          for (const auto* pin : {"CLK", "RST", "WE"}) self->addPin(pin, PinType::INPUT);
      }), width_(width), type_("Register<" + std::to_string(width) + ">"),
      stored_(width, LogicValue::UNKNOWN) {}

void RegisterDirect::evaluate(size_t time, Simulator& simulator) {
    const auto reset = getInputValue("RST");
    const auto clock = getInputValue("CLK");
    if (reset == LogicValue::HIGH) std::fill(stored_.begin(), stored_.end(), LogicValue::LOW);
    else if (reset != LogicValue::LOW) {
        for (auto& bit : stored_) if (bit != LogicValue::LOW) bit = LogicValue::UNKNOWN;
    } else if (previous_clock_ == LogicValue::LOW && clock == LogicValue::HIGH) {
        const auto enable = getInputValue("WE");
        const auto data = getInputPinDynamic("D")->getValueAsVector();
        if (enable == LogicValue::HIGH) {
            for (size_t bit = 0; bit < width_; ++bit)
                stored_[bit] = data[bit] == LogicValue::HIGH || data[bit] == LogicValue::LOW
                    ? data[bit] : LogicValue::UNKNOWN;
        } else if (enable != LogicValue::LOW) {
            for (size_t bit = 0; bit < width_; ++bit)
                stored_[bit] = stored_[bit] == LogicValue::LOW && data[bit] == LogicValue::LOW
                    ? LogicValue::LOW : LogicValue::UNKNOWN;
        }
    }
    previous_clock_ = clock;
    _updateOutputWireDynamic(simulator, "Q", stored_, time);
}
