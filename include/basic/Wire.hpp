#pragma once
#include <string>
#include <memory>
#include <vector>
#include "ForwardDeclarations.hpp"
#include "basic/LogicValue.hpp"

class Wire : public std::enable_shared_from_this<Wire> {
private:
    std::string name;
    LogicValue value;
    std::weak_ptr<Component> owner;
    std::weak_ptr<Pin> source_pin;
    std::vector<std::weak_ptr<Pin>> sink_pins;

public:
    Wire(std::string name);

    std::string getName() const;
    LogicValue getValue() const;
    void setValue(LogicValue new_value);

    void setOwner(std::shared_ptr<Component> component);
    void setSourcePin(std::shared_ptr<Pin> pin);
    void addSinkPin(std::shared_ptr<Pin> pin);

    std::shared_ptr<Component> getOwner() const;
    std::shared_ptr<Pin> getSourcePin() const;
    const std::vector<std::weak_ptr<Pin>>& getSinkPins() const;
    std::string getID() const;

    std::vector<std::shared_ptr<Pin>> getSinkPinsForPython() const {
        std::vector<std::shared_ptr<Pin>> strong_pins;
        strong_pins.reserve(sink_pins.size());
        for (const auto& weak_pin : sink_pins) {
            if (auto strong_pin = weak_pin.lock()) {
                strong_pins.push_back(strong_pin);
            }
        }
        return strong_pins;
    }

    void propagateChange(Simulator& simulator, size_t propagation_time);
};