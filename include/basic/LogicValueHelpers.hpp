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

// The gate rules, as functions. A behavioral implementation that replaces a
// gate network uses these so that unknown inputs give the same result as the
// gates would: a LOW input forces AND low, a HIGH input forces OR high.
inline LogicValue notGate(LogicValue a) {
    return a == LogicValue::HIGH ? LogicValue::LOW
         : a == LogicValue::LOW ? LogicValue::HIGH : LogicValue::UNKNOWN;
}
inline LogicValue andGate(LogicValue a, LogicValue b) {
    if (a == LogicValue::LOW || b == LogicValue::LOW) return LogicValue::LOW;
    return a == LogicValue::HIGH && b == LogicValue::HIGH ? LogicValue::HIGH : LogicValue::UNKNOWN;
}
inline LogicValue orGate(LogicValue a, LogicValue b) {
    if (a == LogicValue::HIGH || b == LogicValue::HIGH) return LogicValue::HIGH;
    return a == LogicValue::LOW && b == LogicValue::LOW ? LogicValue::LOW : LogicValue::UNKNOWN;
}
inline LogicValue xorGate(LogicValue a, LogicValue b) {
    const bool known = (a == LogicValue::LOW || a == LogicValue::HIGH)
                    && (b == LogicValue::LOW || b == LogicValue::HIGH);
    return !known ? LogicValue::UNKNOWN : a == b ? LogicValue::LOW : LogicValue::HIGH;
}
// Mux2to1 as built: (A AND NOT SEL) OR (B AND SEL).
inline LogicValue mux2(LogicValue a, LogicValue b, LogicValue select) {
    return orGate(andGate(a, notGate(select)), andGate(b, select));
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
