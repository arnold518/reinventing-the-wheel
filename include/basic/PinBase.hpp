#pragma once

#include "ForwardDeclarations.hpp"
#include "basic/LogicValue.hpp"
#include "basic/PackedLogic.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class PinType {
    INPUT,
    OUTPUT
};

// A pin of a run-time width (1 to 64 bits) holding one packed four-state
// value. Pin<W> is a typed handle on it; the value and its accessors live here.
class PinBase {
protected:
    std::string name;
    PinType type;
    std::weak_ptr<Component> owner;
    std::weak_ptr<WireBase> external_wire;
    std::weak_ptr<WireBase> internal_wire;
    PackedLogic value_;
    size_t width_;

public:
    PinBase(std::string pin_name, PinType pin_type, std::shared_ptr<Component> owner_comp, size_t width);
    virtual ~PinBase() = default;

    std::string getName() const;
    PinType getType() const;
    std::shared_ptr<Component> getOwner() const;
    std::shared_ptr<WireBase> getExternalWireBase() const;
    std::shared_ptr<WireBase> getInternalWireBase() const;

    void connectExternalBase(const std::shared_ptr<WireBase>& wire);
    void connectInternalBase(const std::shared_ptr<WireBase>& wire);

    size_t getWidth() const { return width_; }
    LogicValue getBit(size_t index) const;
    void setBit(size_t index, LogicValue value);
    uint64_t getValueAsUInt64() const { return value_.highBits(); }
    std::vector<LogicValue> getValueAsVector() const { return value_.toVector(width_); }
    void setValueFromUInt64(uint64_t value) { value_ = PackedLogic::fromUInt64(value, width_); }
    void setValueFromVector(const std::vector<LogicValue>& values) { value_ = PackedLogic::fromVector(values, width_); }
    const PackedLogic& packed() const { return value_; }
    // `packed` must already be this pin's width (bits above it zero).
    void setPacked(const PackedLogic& packed) { value_ = packed; }

    LogicValue getValue() const;
    std::string getID() const;
};
