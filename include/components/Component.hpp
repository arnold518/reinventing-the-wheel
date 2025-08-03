#pragma once
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <type_traits>
#include "ForwardDeclarations.hpp"

class Component : public std::enable_shared_from_this<Component>
{
protected:
    std::string name;
    std::weak_ptr<Component> parent;
    std::vector<std::shared_ptr<Component>> children;
    std::vector<std::shared_ptr<Wire>> wires;

public:
    Component(std::string name);
    virtual ~Component() = default;
    static constexpr const char* TypeName = "Component";
    virtual const char* getTypeName() const { return TypeName; }

    std::string getName() const;
    std::string getID() const;

    std::shared_ptr<Component> getParent() const;
    const std::vector<std::shared_ptr<Component>>& getChildren() const;
    const std::vector<std::shared_ptr<Wire>>& getWires() const;

    void addChild(const std::shared_ptr<Component>& child);
    void addWire(const std::shared_ptr<Wire>& wire);

    std::string format(int lvl, bool formatWires, bool formatPins) const;
};