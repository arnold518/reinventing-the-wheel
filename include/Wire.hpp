#pragma once
#include <string>
#include <memory>
#include <vector>
#include "ForwardDeclarations.hpp"
#include "LogicValue.hpp"

class Wire : public std::enable_shared_from_this<Wire> {
private:
    std::string name;
    LogicValue value;
    std::weak_ptr<Pin> source_pin;
    std::vector<std::weak_ptr<Pin>> sink_pins;

public:
    Wire(std::string name);

    std::string getName() const;
    LogicValue getValue() const;
    void setValue(LogicValue new_value);

    void setSourcePin(std::shared_ptr<Pin> pin);
    void addSinkPin(std::shared_ptr<Pin> pin);

    std::shared_ptr<Pin> getSourcePin() const;
    const std::vector<std::weak_ptr<Pin>>& getSinkPins() const;

    void propagateChange(Simulator& simulator, size_t propagation_time);
};