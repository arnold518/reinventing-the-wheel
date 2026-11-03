#pragma once

#include "basic/LogicValue.hpp"
#include "basic/PackedLogic.hpp"
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
// Mux<2,1> as built: (A AND NOT SEL) OR (B AND SEL).
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

// The same gate rules on every bit of a packed word at once (HIGH_Z reads as
// UNKNOWN; an undecided result bit is UNKNOWN). `width` bounds the result.
namespace packed {
inline uint64_t ones(const PackedLogic& v) { return v.value & ~v.unknown; }
inline uint64_t zeros(const PackedLogic& v, size_t width) { return ~v.value & ~v.unknown & PackedLogic::mask(width); }
inline PackedLogic from(uint64_t one, uint64_t zero, size_t width) {
    return {one, PackedLogic::mask(width) & ~(one | zero)};
}
inline PackedLogic notGate(const PackedLogic& a, size_t width) { return from(zeros(a, width), ones(a), width); }
inline PackedLogic andGate(const PackedLogic& a, const PackedLogic& b, size_t width) {
    return from(ones(a) & ones(b), zeros(a, width) | zeros(b, width), width);
}
inline PackedLogic orGate(const PackedLogic& a, const PackedLogic& b, size_t width) {
    return from(ones(a) | ones(b), zeros(a, width) & zeros(b, width), width);
}
inline PackedLogic xorGate(const PackedLogic& a, const PackedLogic& b, size_t width) {
    const uint64_t known = PackedLogic::mask(width) & ~a.unknown & ~b.unknown;
    const uint64_t differ = a.value ^ b.value;
    return from(known & differ, known & ~differ, width);
}
} // namespace packed
} // namespace circuit::logic_values
