#pragma once

#include "simulator/SimulationTime.hpp"

#include "ForwardDeclarations.hpp"
#include "basic/LogicValue.hpp"
#include "basic/PackedLogic.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class BasicComponent;

// A wire of a run-time width (1 to 64 bits) holding one packed four-state
// value. Wire<W> is a typed handle on it; the value and its accessors live here.
class WireBase {
public:
    // What a sink does when the wire changes, classified once per sink:
    // evaluate its basic part, forward into or out of a composite, or nothing.
    // propagateWireChange checks the cache against the live sink list on every
    // call and rebuilds it when the list has changed.
    struct SinkRoute {
        std::weak_ptr<PinBase> pin;
        std::weak_ptr<BasicComponent> evaluated;
        bool forwards = false;
    };
    std::vector<SinkRoute>& sinkRoutes() const { return sink_routes_; }

private:
    mutable std::vector<SinkRoute> sink_routes_;

protected:
    std::string name;
    std::weak_ptr<Component> owner;
    std::weak_ptr<PinBase> source_pin;
    std::vector<std::weak_ptr<PinBase>> sink_pins;
    PackedLogic value_;
    size_t width_;

public:
    WireBase(std::string wire_name, size_t width);
    virtual ~WireBase() = default;

    std::string getName() const;
    void setOwner(std::shared_ptr<Component> component);
    std::shared_ptr<Component> getOwner() const;

    void setSourcePinBase(std::shared_ptr<PinBase> pin);
    void addSinkPinBase(std::shared_ptr<PinBase> pin);
    std::shared_ptr<PinBase> getSourcePinBase() const;
    const std::vector<std::weak_ptr<PinBase>>& getSinkPinsBase() const;
    std::vector<std::shared_ptr<PinBase>> getSinkPinsBaseForPython() const;

    size_t getWidth() const { return width_; }
    LogicValue getBit(size_t index) const;
    void setBit(size_t index, LogicValue value);
    uint64_t getValue() const { return value_.highBits(); }
    std::vector<LogicValue> getValueVector() const { return value_.toVector(width_); }
    void setValue(uint64_t value) { value_ = PackedLogic::fromUInt64(value, width_); }
    void setValueVector(const std::vector<LogicValue>& values) { value_ = PackedLogic::fromVector(values, width_); }
    LogicValue getSingleValue() const { return value_.get(0); }
    void setSingleValue(LogicValue value) { value_.set(0, value); }
    const PackedLogic& packed() const { return value_; }
    // `packed` must already be this wire's width (bits above it zero).
    void setPacked(const PackedLogic& packed) { value_ = packed; }
    // Delivers the current value to every sink (propagateWireChange).
    void propagateChange(Simulator& simulator, SimulationTime propagation_time);

    std::string getID() const;
};

void propagateWireChange(WireBase& wire, Simulator& simulator, SimulationTime propagation_time);
