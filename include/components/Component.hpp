#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "ForwardDeclarations.hpp"
#include "components/selection/SelectionTypes.hpp"

class ComponentBuilder;

class Component : public std::enable_shared_from_this<Component>
{
protected:
    std::string name;
    std::weak_ptr<Component> parent;
    std::vector<std::shared_ptr<Component>> children;
    std::vector<std::shared_ptr<Wire<>>> wires;
    std::vector<std::shared_ptr<WireBase>> all_wires;
    std::optional<circuit::ComponentInstanceMetadata> instance_metadata_;

public:
    Component(std::string name);
    virtual ~Component() = default;
    static constexpr const char* TypeName = "Component";
    virtual const char* getTypeName() const { return TypeName; }

    template<typename T, typename... Args>
    static std::shared_ptr<T> create(Args&&... args);

    template<typename T, typename... Args>
    static std::shared_ptr<T> createWithContext(
        const std::shared_ptr<circuit::BuildContext>& context,
        Args&&... args);

    virtual void buildInternals(ComponentBuilder& builder);

    std::string getName() const;
    std::string getID() const;
    std::shared_ptr<Component> getParent() const;
    const std::vector<std::shared_ptr<Component>>& getChildren() const;
    const std::vector<std::shared_ptr<Wire<>>>& getWires() const;
    const std::vector<std::shared_ptr<WireBase>>& getAllWires() const;

    void setInstanceMetadata(circuit::ComponentInstanceMetadata metadata);
    const std::optional<circuit::ComponentInstanceMetadata>& getInstanceMetadata() const;
    std::string getContractId() const;
    uint32_t getContractVersion() const;
    std::string getImplementationId() const;
    std::string getSelectedFidelity() const;
    std::vector<std::string> getAvailableFidelities() const;
    bool isProfileSelectable() const;
    bool isTerminalPrimitive() const;
    bool usedUnavailableFidelityException() const;
    std::string getSelectionReason() const;
    std::string getProfileFingerprint() const;
    
    bool isAncestorOf(const std::shared_ptr<const Component>& other) const;
    static std::shared_ptr<Component> findLCA(const std::vector<std::shared_ptr<Component>>& components);

    void addChild(const std::shared_ptr<Component>& child);
    void addWire(const std::shared_ptr<WireBase>& wire);

    std::string format(int lvl, bool formatWires, bool formatPins) const;
};

#include "components/Component.tpp"
