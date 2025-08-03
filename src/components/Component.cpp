#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/BasicComponent.hpp"
#include "basic/Wire.hpp"
#include <iostream>
#include <algorithm>
#include <sstream>

Component::Component(std::string name) : name(std::move(name)) {}

std::string Component::getName() const { return name; }

std::shared_ptr<Component> Component::getParent() const { return parent.lock(); }
const std::vector<std::shared_ptr<Component>>& Component::getChildren() const { return children; }
const std::vector<std::shared_ptr<Wire>>& Component::getWires() const { return wires; }

std::string Component::getID() const
{
    std::string id_path = name;
    std::weak_ptr<Component> current_parent_weak = parent;
    while (auto current_parent = current_parent_weak.lock())
    {
        id_path = current_parent->getName() + " > " + id_path;
        current_parent_weak = current_parent->parent;
    }
    return id_path;
}

void Component::addChild(const std::shared_ptr<Component>& child)
{
    if (!child || child.get() == this) return;

    if (auto old_parent = child->parent.lock())
    {
        auto& siblings = old_parent->children;
        siblings.erase(std::remove_if(siblings.begin(), siblings.end(), 
                                      [&](const std::shared_ptr<Component>& c){ return !c || c == child; }),
                        siblings.end());
    }

    children.push_back(child);
    child->parent = shared_from_this();
}

void Component::addWire(const std::shared_ptr<Wire>& wire)
{
    wire->setOwner(shared_from_this());
    wires.push_back(wire);
}

std::string Component::format(int lvl, bool formatWires, bool formatPins) const
{
    std::stringstream ss;
    std::string indent(lvl * 2, ' ');
    std::string type = getTypeName();

    ss << indent << "(" << type << ") '" << getID() << "'\n";

    if (formatWires)
    {
        for (const auto& wire : wires) if(wire)
        {
            ss << indent << "  (Wire) '" << wire->getName() << "' : " << wire->getValue() << "\n";
        }
    }
    if (formatPins) if (auto io = std::dynamic_pointer_cast<const IOComponent>(shared_from_this()))
    {
        ss << io->formatPins(lvl);
    }

    for (const auto& child : children) if(child)
    {
        ss << child->format(lvl + 1, formatWires, formatPins);
    }

    return ss.str();
}