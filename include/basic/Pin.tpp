#pragma once

#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include <algorithm>
#include <stdexcept>

template<size_t WIDTH>
Pin<WIDTH>::Pin(std::string pin_name, PinType pin_type, std::shared_ptr<Component> owner_comp)
    : PinBase(std::move(pin_name), pin_type, std::move(owner_comp)) {
    value.fill(LogicValue::UNKNOWN);
}

template<size_t WIDTH>
std::shared_ptr<Wire<WIDTH>> Pin<WIDTH>::getExternalWire() const {
    return std::dynamic_pointer_cast<Wire<WIDTH>>(external_wire.lock());
}

template<size_t WIDTH>
std::shared_ptr<Wire<WIDTH>> Pin<WIDTH>::getInternalWire() const {
    return std::dynamic_pointer_cast<Wire<WIDTH>>(internal_wire.lock());
}

template<size_t WIDTH>
void Pin<WIDTH>::connectExternal(const std::shared_ptr<Wire<WIDTH>>& wire) {
    connectExternalBase(wire);
}

template<size_t WIDTH>
void Pin<WIDTH>::connectInternal(const std::shared_ptr<Wire<WIDTH>>& wire) {
    connectInternalBase(wire);
}

template<size_t WIDTH>
size_t Pin<WIDTH>::getWidth() const {
    return WIDTH;
}

template<size_t WIDTH>
LogicValue Pin<WIDTH>::getBit(size_t index) const {
    if (index >= WIDTH) {
        throw std::out_of_range("Pin bit index out of range");
    }
    return value[index];
}

template<size_t WIDTH>
void Pin<WIDTH>::setBit(size_t index, LogicValue bit_value) {
    if (index >= WIDTH) {
        throw std::out_of_range("Pin bit index out of range");
    }
    value[index] = bit_value;
}

template<size_t WIDTH>
uint64_t Pin<WIDTH>::getValueAsUInt64() const {
    uint64_t result = 0;
    constexpr size_t usable_width = std::min<size_t>(WIDTH, 64);
    for (size_t i = 0; i < usable_width; ++i) {
        if (value[i] == LogicValue::HIGH) {
            result |= (uint64_t{1} << i);
        }
    }
    return result;
}

template<size_t WIDTH>
std::vector<LogicValue> Pin<WIDTH>::getValueAsVector() const {
    return {value.begin(), value.end()};
}

template<size_t WIDTH>
void Pin<WIDTH>::setValueFromUInt64(uint64_t new_value) {
    for (size_t i = 0; i < WIDTH; ++i) {
        value[i] = ((new_value >> i) & 1U) ? LogicValue::HIGH : LogicValue::LOW;
    }
}

template<size_t WIDTH>
void Pin<WIDTH>::setValueFromVector(const std::vector<LogicValue>& values) {
    for (size_t i = 0; i < WIDTH; ++i) {
        value[i] = (i < values.size()) ? values[i] : LogicValue::UNKNOWN;
    }
}
