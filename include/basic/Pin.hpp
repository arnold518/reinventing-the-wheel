#pragma once
#include <string>
#include <memory>
#include "ForwardDeclarations.hpp"
#include "basic/LogicValue.hpp"

enum class PinType {
    INPUT,
    OUTPUT
};

class Pin : public std::enable_shared_from_this<Pin> {
private:
    std::string name;
    PinType type;
    std::weak_ptr<Component> owner;
    std::weak_ptr<Wire> connected_wire;

public:
    Pin(std::string name, PinType type, std::shared_ptr<Component> owner_comp);

    std::string getName() const;
    PinType getType() const;
    std::shared_ptr<Component> getOwner() const;
    std::shared_ptr<Wire> getConnectedWire() const;

    void connect(const std::shared_ptr<Wire>& wire);
    LogicValue getValue() const;
    std::string getID() const;
};