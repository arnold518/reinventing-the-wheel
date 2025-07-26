#pragma once
#include <string>
#include <vector>
#include <memory>
#include "ForwardDeclarations.hpp"

class Component : public std::enable_shared_from_this<Component>
{
protected:
    std::string name;
    std::weak_ptr<Component> parent;
    std::vector<std::shared_ptr<Component>> children;

public:
    Component(std::string name);
    virtual ~Component() = default;

    std::string getName() const;
    std::string getID() const;

    std::shared_ptr<Component> getParent() const;
    const std::vector<std::shared_ptr<Component>>& getChildren() const;

    void addChild(const std::shared_ptr<Component>& child);
};