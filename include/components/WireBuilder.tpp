#pragma once

#include "WireBuilder.hpp"
#include "ComponentBuilder.hpp"

template<typename CompType>
WireBuilder& WireBuilder::from(const std::string& comp_name, const std::string& pin_name) {
    source_pin_ = builder_->getOutputPin<CompType>(comp_name, pin_name);
    return *this;
}

template<typename CompType>
WireBuilder& WireBuilder::to(const std::string& comp_name, const std::string& pin_name) {
    sink_pins_.push_back(builder_->getInputPin<CompType>(comp_name, pin_name));
    return *this;
}
