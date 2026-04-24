#pragma once

#include "basic/Wire.hpp"
#include "basic/Pin.hpp"
#include <algorithm>
#include <stdexcept>

template<size_t WIDTH>
Wire<WIDTH>::Wire(std::string wire_name)
    : WireBase(std::move(wire_name)) {
    value.fill(LogicValue::UNKNOWN);
}

template<size_t WIDTH>
void Wire<WIDTH>::setSourcePin(std::shared_ptr<Pin<WIDTH>> pin) {
    setSourcePinBase(pin);
}

template<size_t WIDTH>
void Wire<WIDTH>::addSinkPin(std::shared_ptr<Pin<WIDTH>> pin) {
    addSinkPinBase(pin);
}

template<size_t WIDTH>
std::shared_ptr<Pin<WIDTH>> Wire<WIDTH>::getSourcePin() const {
    return std::dynamic_pointer_cast<Pin<WIDTH>>(source_pin.lock());
}

template<size_t WIDTH>
std::vector<std::weak_ptr<Pin<WIDTH>>> Wire<WIDTH>::getSinkPins() const {
    std::vector<std::weak_ptr<Pin<WIDTH>>> pins;
    pins.reserve(sink_pins.size());
    for (const auto& weak_pin : sink_pins) {
        if (auto pin = std::dynamic_pointer_cast<Pin<WIDTH>>(weak_pin.lock())) {
            pins.push_back(pin);
        }
    }
    return pins;
}

template<size_t WIDTH>
std::vector<std::shared_ptr<Pin<WIDTH>>> Wire<WIDTH>::getSinkPinsForPython() const {
    std::vector<std::shared_ptr<Pin<WIDTH>>> pins;
    pins.reserve(sink_pins.size());
    for (const auto& weak_pin : sink_pins) {
        if (auto pin = std::dynamic_pointer_cast<Pin<WIDTH>>(weak_pin.lock())) {
            pins.push_back(pin);
        }
    }
    return pins;
}

template<size_t WIDTH>
size_t Wire<WIDTH>::getWidth() const {
    return WIDTH;
}

template<size_t WIDTH>
LogicValue Wire<WIDTH>::getBit(size_t index) const {
    if (index >= WIDTH) {
        throw std::out_of_range("Wire bit index out of range");
    }
    return value[index];
}

template<size_t WIDTH>
void Wire<WIDTH>::setBit(size_t index, LogicValue bit_value) {
    if (index >= WIDTH) {
        throw std::out_of_range("Wire bit index out of range");
    }
    value[index] = bit_value;
}

template<size_t WIDTH>
uint64_t Wire<WIDTH>::getValue() const {
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
std::vector<LogicValue> Wire<WIDTH>::getValueVector() const {
    return {value.begin(), value.end()};
}

template<size_t WIDTH>
void Wire<WIDTH>::setValue(uint64_t new_value) {
    for (size_t i = 0; i < WIDTH; ++i) {
        value[i] = ((new_value >> i) & 1U) ? LogicValue::HIGH : LogicValue::LOW;
    }
}

template<size_t WIDTH>
void Wire<WIDTH>::setValue(LogicValue new_value) {
    setSingleValue(new_value);
}

template<size_t WIDTH>
void Wire<WIDTH>::setValueVector(const std::vector<LogicValue>& values) {
    for (size_t i = 0; i < WIDTH; ++i) {
        value[i] = (i < values.size()) ? values[i] : LogicValue::UNKNOWN;
    }
}

template<size_t WIDTH>
LogicValue Wire<WIDTH>::getSingleValue() const {
    return value[0];
}

template<size_t WIDTH>
void Wire<WIDTH>::setSingleValue(LogicValue new_value) {
    value[0] = new_value;
}

template<size_t WIDTH>
void Wire<WIDTH>::propagateChange(Simulator& simulator, size_t propagation_time) {
    propagateWireChange(*this, simulator, propagation_time);
}
