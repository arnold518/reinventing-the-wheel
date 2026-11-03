#pragma once
#include <cstdint>
#include <ostream>

// One byte per bit: every stored value array and vector is a quarter of the
// int-sized default.
enum class LogicValue : uint8_t
{
    LOW = 0,
    HIGH = 1,
    UNKNOWN,
    HIGH_Z
};

inline std::ostream& operator<<(std::ostream& os, LogicValue val) {
    switch (val) {
        case LogicValue::LOW: return os << "0";
        case LogicValue::HIGH: return os << "1";
        case LogicValue::UNKNOWN: return os << "X";
        case LogicValue::HIGH_Z: return os << "Z";
        default: return os << "?";
    }
}