#pragma once

#include "basic/Wire.hpp"
#include "basic/Pin.hpp"

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
