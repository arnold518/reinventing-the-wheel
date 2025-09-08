#pragma once

#include <string>
#include <vector>
#include <memory>
#include "ForwardDeclarations.hpp"

class ComponentBuilder;

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

    template<typename T, typename... Args>
    static std::shared_ptr<T> create(Args&&... args);

    virtual void buildInternals(ComponentBuilder& builder);

    std::string getName() const;
    std::string getID() const;
    std::shared_ptr<Component> getParent() const;
    const std::vector<std::shared_ptr<Component>>& getChildren() const;
    const std::vector<std::shared_ptr<Wire>>& getWires() const;
    
    bool isAncestorOf(const std::shared_ptr<const Component>& other) const;
    static std::shared_ptr<Component> findLCA(const std::vector<std::shared_ptr<Component>>& components);

    void addChild(const std::shared_ptr<Component>& child);
    void addWire(const std::shared_ptr<Wire>& wire);

    std::string format(int lvl, bool formatWires, bool formatPins) const;
};

#include "components/Component.tpp"