#include "components/WireBuilder.hpp"
#include "components/ComponentBuilder.hpp"
#include "basic/Pin.hpp"
#include "basic/Wire.hpp"

WireBuilder::WireBuilder(ComponentBuilder* builder, std::string name)
    : builder_(builder), wire_name_(std::move(name)) {}

WireBuilder::WireBuilder(WireBuilder&& other) noexcept
    : builder_(other.builder_),
      wire_name_(std::move(other.wire_name_)),
      source_pin_(std::move(other.source_pin_)),
      sink_pins_(std::move(other.sink_pins_)),
      built_(other.built_) {
    other.built_ = true; // Prevent other from building on destruction
}

WireBuilder& WireBuilder::operator=(WireBuilder&& other) noexcept {
    if (this != &other) {
        builder_ = other.builder_;
        wire_name_ = std::move(other.wire_name_);
        source_pin_ = std::move(other.source_pin_);
        sink_pins_ = std::move(other.sink_pins_);
        built_ = other.built_;
        other.built_ = true;
    }
    return *this;
}

WireBuilder::~WireBuilder() {
    // Auto-commit on destruction if not already built
    if (!built_ && source_pin_ && !sink_pins_.empty()) {
        try {
            build();
        } catch (...) {
            // Suppress exceptions in destructor
        }
    }
}

WireBuilder& WireBuilder::from(std::shared_ptr<Pin> pin) {
    source_pin_ = pin;
    return *this;
}

WireBuilder& WireBuilder::fromInput(const std::string& pin_name) {
    source_pin_ = builder_->getInputPin(pin_name);
    return *this;
}

WireBuilder& WireBuilder::fromOutput(const std::string& pin_name) {
    source_pin_ = builder_->getOutputPin(pin_name);
    return *this;
}

WireBuilder& WireBuilder::to(std::shared_ptr<Pin> pin) {
    sink_pins_.push_back(pin);
    return *this;
}

WireBuilder& WireBuilder::toOutput(const std::string& pin_name) {
    sink_pins_.push_back(builder_->getOutputPin(pin_name));
    return *this;
}

WireBuilder& WireBuilder::toInput(const std::string& pin_name) {
    sink_pins_.push_back(builder_->getInputPin(pin_name));
    return *this;
}

std::shared_ptr<Wire> WireBuilder::build() {
    if (built_) {
        throw std::runtime_error("WireBuilder::build() called twice on the same builder");
    }
    if (!source_pin_) {
        throw std::runtime_error("WireBuilder: No source pin specified for wire '" + wire_name_ + "'");
    }
    if (sink_pins_.empty()) {
        throw std::runtime_error("WireBuilder: No sink pins specified for wire '" + wire_name_ + "'");
    }

    built_ = true;
    return builder_->addNewWire(wire_name_, source_pin_, sink_pins_);
}
