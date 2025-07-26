#include "Component.hpp"
#include <iostream>
#include <algorithm>

Component::Component(std::string name) : name(std::move(name)) {}

std::string Component::getName() const { return name; }

std::shared_ptr<Component> Component::getParent() const { return parent.lock(); }
const std::vector<std::shared_ptr<Component>>& Component::getChildren() const { return children; }

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