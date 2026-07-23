#include "modules/memory/Register32Direct.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "modules/memory/Register32.hpp"
#include <utility>

Register32Direct::Register32Direct(std::string name)
    : BasicComponent(std::move(name), 1,
                     circuit::families::Register32.pinInitializer()),
      stored_value_(32, LogicValue::UNKNOWN) {}

void Register32Direct::evaluate(size_t current_time,
                                          Simulator& simulator) {
    const auto we = getInputValue("WE");
    const auto clk = getInputValue("CLK");
    const auto rst = getInputValue("RST");

    if (rst == LogicValue::HIGH) {
        stored_value_.assign(32, LogicValue::LOW);
    } else if (rst != LogicValue::LOW) {
        stored_value_.assign(32, LogicValue::UNKNOWN);
    } else if (previous_clk_ == LogicValue::LOW && clk == LogicValue::HIGH) {
        if (we == LogicValue::HIGH) {
            const auto input = getInputPin<32>("D")->getValueAsVector();
            for (size_t bit = 0; bit < stored_value_.size(); ++bit) {
                stored_value_[bit] = input[bit] == LogicValue::HIGH
                    || input[bit] == LogicValue::LOW
                    ? input[bit]
                    : LogicValue::UNKNOWN;
            }
        } else if (we != LogicValue::LOW) {
            stored_value_.assign(32, LogicValue::UNKNOWN);
        }
    }

    previous_clk_ = clk;
    _updateOutputWire<32>(simulator, "Q", stored_value_, current_time);
}
