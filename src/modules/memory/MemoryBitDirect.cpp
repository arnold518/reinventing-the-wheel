#include "modules/memory/MemoryBitDirect.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "modules/memory/MemoryBit.hpp"

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

MemoryBitDirect::MemoryBitDirect(std::string name)
    : BasicComponent(std::move(name), 1,
                     circuit::families::MemoryBit.pinInitializer()),
      stored_value(LogicValue::UNKNOWN),
      previous_clk(LogicValue::UNKNOWN) {}

void MemoryBitDirect::evaluate(size_t current_time, Simulator& simulator) {
    const auto d = getInputValue("D");
    const auto we = getInputValue("WE");
    const auto clk = getInputValue("CLK");
    const auto rst = getInputValue("RST");

    if (rst == LogicValue::HIGH) {
        stored_value = LogicValue::LOW;
    } else if (rst != LogicValue::LOW) {
        stored_value = mergePossibleValues(
            stored_value, LogicValue::LOW);
    } else if (previous_clk == LogicValue::LOW && clk == LogicValue::HIGH) {
        if (we == LogicValue::HIGH) {
            stored_value = knownOrUnknown(d);
        } else if (we != LogicValue::LOW) {
            stored_value = mergePossibleValues(
                stored_value, d);
        }
    }

    previous_clk = clk;
    _updateOutputWire(simulator, "Q", stored_value, current_time);
}
