#pragma once

#include "ForwardDeclarations.hpp"
#include "basic/LogicValue.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class WireBase {
protected:
    std::string name;
    std::weak_ptr<Component> owner;
    std::weak_ptr<PinBase> source_pin;
    std::vector<std::weak_ptr<PinBase>> sink_pins;

public:
    explicit WireBase(std::string wire_name);
    virtual ~WireBase() = default;

    std::string getName() const;
    void setOwner(std::shared_ptr<Component> component);
    std::shared_ptr<Component> getOwner() const;

    void setSourcePinBase(std::shared_ptr<PinBase> pin);
    void addSinkPinBase(std::shared_ptr<PinBase> pin);
    std::shared_ptr<PinBase> getSourcePinBase() const;
    const std::vector<std::weak_ptr<PinBase>>& getSinkPinsBase() const;
    std::vector<std::shared_ptr<PinBase>> getSinkPinsBaseForPython() const;

    virtual size_t getWidth() const = 0;
    virtual LogicValue getBit(size_t index) const = 0;
    virtual void setBit(size_t index, LogicValue value) = 0;
    virtual uint64_t getValue() const = 0;
    virtual std::vector<LogicValue> getValueVector() const = 0;
    virtual void setValue(uint64_t value) = 0;
    virtual void setValueVector(const std::vector<LogicValue>& values) = 0;
    virtual LogicValue getSingleValue() const = 0;
    virtual void setSingleValue(LogicValue value) = 0;
    virtual void propagateChange(Simulator& simulator, size_t propagation_time) = 0;

    std::string getID() const;
};

void propagateWireChange(WireBase& wire, Simulator& simulator, size_t propagation_time);
