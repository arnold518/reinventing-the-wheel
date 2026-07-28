#include "modules/memory/Register32Direct.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "modules/memory/Register32.hpp"
#include <utility>

namespace {
LogicValue knownOrUnknown(LogicValue value) {
    return value == LogicValue::HIGH || value == LogicValue::LOW
        ? value
        : LogicValue::UNKNOWN;
}

LogicValue mergePossibleValues(
    LogicValue first,
    LogicValue second) {
    const auto known_first = knownOrUnknown(first);
    const auto known_second = knownOrUnknown(second);
    return known_first == known_second
        ? known_first
        : LogicValue::UNKNOWN;
}
}

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
        for (auto& value : stored_value_) {
            value = mergePossibleValues(
                value, LogicValue::LOW);
        }
    } else if (previous_clk_ == LogicValue::LOW && clk == LogicValue::HIGH) {
        const auto input = getInputPin<32>("D")->getValueAsVector();
        if (we == LogicValue::HIGH) {
            for (size_t bit = 0; bit < stored_value_.size(); ++bit) {
                stored_value_[bit] = knownOrUnknown(input[bit]);
            }
        } else if (we != LogicValue::LOW) {
            for (size_t bit = 0; bit < stored_value_.size(); ++bit) {
                stored_value_[bit] = mergePossibleValues(
                    stored_value_[bit], input[bit]);
            }
        }
    }

    previous_clk_ = clk;
    _updateOutputWire<32>(simulator, "Q", stored_value_, current_time);
}
