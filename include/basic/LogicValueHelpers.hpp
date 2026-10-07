#pragma once

#include "basic/LogicValue.hpp"
#include <vector>

// Small four-state predicates shared by directly evaluated models.
namespace circuit::logic_values {
inline bool high(LogicValue value) {
    return value == LogicValue::HIGH;
}

inline LogicValue logic(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}

// HIGH_Z and UNKNOWN both read as UNKNOWN.
inline LogicValue sanitizeBit(LogicValue value) {
    return value == LogicValue::HIGH || value == LogicValue::LOW
        ? value
        : LogicValue::UNKNOWN;
}

inline bool allKnown(const std::vector<LogicValue>& values) {
    for (const auto value : values) {
        if (value != LogicValue::LOW && value != LogicValue::HIGH) {
            return false;
        }
    }
    return true;
}
} // namespace circuit::logic_values
