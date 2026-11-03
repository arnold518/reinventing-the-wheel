#pragma once

#include "basic/Pin.hpp"
#include "basic/Wire.hpp"

template<size_t WIDTH>
std::shared_ptr<Wire<WIDTH>> Pin<WIDTH>::getExternalWire() const {
    return std::dynamic_pointer_cast<Wire<WIDTH>>(external_wire.lock());
}

template<size_t WIDTH>
std::shared_ptr<Wire<WIDTH>> Pin<WIDTH>::getInternalWire() const {
    return std::dynamic_pointer_cast<Wire<WIDTH>>(internal_wire.lock());
}
