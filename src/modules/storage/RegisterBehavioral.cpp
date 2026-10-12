#include "modules/storage/RegisterBehavioral.hpp"
#include "modules/storage/Register.hpp"

RegisterBehavioral::RegisterBehavioral(std::string name, size_t width)
    : BasicComponent(std::move(name), 1, [width](IOComponent* self) {
          self->addPinDynamic("D", PinType::INPUT, width);
          self->addPinDynamic("Q", PinType::OUTPUT, width);
          for (const auto* pin : {"CLK", "RST", "WE"}) self->addPin(pin, PinType::INPUT);
      }), width_(width), type_("Register<" + std::to_string(width) + ">"),
      stored_(width, LogicValue::UNKNOWN) {}

void registerStep(std::vector<LogicValue>& stored, LogicValue& previous_clock, LogicValue reset, LogicValue clock,
                  LogicValue enable, const std::vector<LogicValue>& data) {
    if (reset == LogicValue::HIGH) std::fill(stored.begin(), stored.end(), LogicValue::LOW);
    else if (reset != LogicValue::LOW) {
        for (auto& bit : stored) if (bit != LogicValue::LOW) bit = LogicValue::UNKNOWN;
    } else if (previous_clock == LogicValue::LOW && clock == LogicValue::HIGH) {
        if (enable == LogicValue::HIGH) {
            for (size_t bit = 0; bit < stored.size(); ++bit)
                stored[bit] = data[bit] == LogicValue::HIGH || data[bit] == LogicValue::LOW
                    ? data[bit] : LogicValue::UNKNOWN;
        } else if (enable != LogicValue::LOW) {
            for (size_t bit = 0; bit < stored.size(); ++bit)
                stored[bit] = stored[bit] == LogicValue::LOW && data[bit] == LogicValue::LOW
                    ? LogicValue::LOW : LogicValue::UNKNOWN;
        }
    }
    previous_clock = clock;
}

void RegisterBehavioral::evaluate(size_t time, Simulator& simulator) {
    registerStep(stored_, previous_clock_, getInputValue("RST"), getInputValue("CLK"), getInputValue("WE"),
                 getInputPinDynamic("D")->getValueAsVector());
    _updateOutputWireDynamic(simulator, "Q", stored_, time);
}
