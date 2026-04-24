#pragma once

#include "ForwardDeclarations.hpp"
#include "basic/LogicValue.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

enum class PinType {
    INPUT,
    OUTPUT
};

class PinBase {
protected:
    std::string name;
    PinType type;
    std::weak_ptr<Component> owner;
    std::weak_ptr<WireBase> external_wire;
    std::weak_ptr<WireBase> internal_wire;

public:
    PinBase(std::string pin_name, PinType pin_type, std::shared_ptr<Component> owner_comp);
    virtual ~PinBase() = default;

    std::string getName() const;
    PinType getType() const;
    std::shared_ptr<Component> getOwner() const;
    std::shared_ptr<WireBase> getExternalWireBase() const;
    std::shared_ptr<WireBase> getInternalWireBase() const;

    void connectExternalBase(const std::shared_ptr<WireBase>& wire);
    void connectInternalBase(const std::shared_ptr<WireBase>& wire);

    virtual size_t getWidth() const = 0;
    virtual LogicValue getBit(size_t index) const = 0;
    virtual void setBit(size_t index, LogicValue value) = 0;
    virtual uint64_t getValueAsUInt64() const = 0;
    virtual std::vector<LogicValue> getValueAsVector() const = 0;
    virtual void setValueFromUInt64(uint64_t value) = 0;
    virtual void setValueFromVector(const std::vector<LogicValue>& values) = 0;

    LogicValue getValue() const;
    std::string getID() const;
};
