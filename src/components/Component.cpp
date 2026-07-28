#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/BasicComponent.hpp"
#include "basic/Wire.hpp"
#include "basic/WireBase.hpp"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <set>

Component::Component(std::string name) : name(std::move(name)) {}

void Component::buildInternals(ComponentBuilder& builder) {
    // Base implementation is empty. To be overridden by composite components.
}

std::string Component::getName() const { return name; }

std::shared_ptr<Component> Component::getParent() const { return parent.lock(); }
const std::vector<std::shared_ptr<Component>>& Component::getChildren() const { return children; }
const std::vector<std::shared_ptr<Wire<>>>& Component::getWires() const { return wires; }
const std::vector<std::shared_ptr<WireBase>>& Component::getAllWires() const { return all_wires; }

void Component::setInstanceMetadata(circuit::ComponentInstanceMetadata metadata) {
    instance_metadata_ = std::move(metadata);
}

const std::optional<circuit::ComponentInstanceMetadata>& Component::getInstanceMetadata() const {
    return instance_metadata_;
}

std::string Component::getContractId() const {
    return instance_metadata_ ? instance_metadata_->contract_id
                              : "explicit." + std::string(getTypeName());
}

uint32_t Component::getContractVersion() const {
    return instance_metadata_ ? instance_metadata_->contract_version : 0;
}

std::string Component::getImplementationId() const {
    return instance_metadata_ ? instance_metadata_->implementation_id
                              : "explicit.construction." + std::string(getTypeName());
}

std::string Component::getSelectedFidelity() const {
    if (instance_metadata_) {
        return circuit::toString(instance_metadata_->fidelity);
    }
    return "unspecified";
}

std::vector<std::string> Component::getAvailableFidelities() const {
    std::vector<std::string> result;
    if (!instance_metadata_) {
        return result;
    }
    result.reserve(instance_metadata_->available_fidelities.size());
    for (const auto fidelity : instance_metadata_->available_fidelities) {
        result.push_back(circuit::toString(fidelity));
    }
    return result;
}

bool Component::isProfileSelectable() const {
    return instance_metadata_
        && instance_metadata_->available_fidelities.size() > 1;
}

bool Component::isTerminalPrimitive() const {
    return instance_metadata_ && instance_metadata_->terminal_primitive;
}

bool Component::usedUnavailableFidelityException() const {
    return instance_metadata_
        && instance_metadata_->used_unavailable_exception;
}

std::string Component::getSelectionReason() const {
    return instance_metadata_ ? instance_metadata_->selection_reason
                              : "explicit construction";
}

std::string Component::getProfileFingerprint() const {
    return instance_metadata_ ? instance_metadata_->profile_fingerprint : "explicit";
}

std::string Component::getID() const
{
    std::string id_path = name;
    auto current_parent_weak = parent;
    while (auto current_parent = current_parent_weak.lock())
    {
        id_path = current_parent->getName() + "." + id_path;
        current_parent_weak = current_parent->parent;
    }
    return id_path;
}

bool Component::isAncestorOf(const std::shared_ptr<const Component>& other) const {
    if (!other) return false;
    auto current = other->getParent();
    while (current) {
        if (current.get() == this) {
            return true;
        }
        current = current->getParent();
    }
    return false;
}

std::shared_ptr<Component> Component::findLCA(const std::vector<std::shared_ptr<Component>>& components) {
    if (components.empty()) return nullptr;
    if (components.size() == 1) return components[0];

    std::set<std::shared_ptr<const Component>> path_to_root;
    auto current = components[0];
    while (current) {
        path_to_root.insert(current);
        current = current->getParent();
    }

    std::shared_ptr<Component> lca = nullptr;
    for (size_t i = 1; i < components.size(); ++i) {
        lca = nullptr;
        current = components[i];
        while (current) {
            if (path_to_root.count(current)) {
                lca = std::const_pointer_cast<Component>(current);
                break;
            }
            current = current->getParent();
        }
        if (!lca) return nullptr; // No common ancestor found for the whole set

        // For the next iteration, the new path is from the current LCA to the root
        path_to_root.clear();
        current = lca;
        while(current) {
            path_to_root.insert(current);
            current = current->getParent();
        }
    }
    return lca;
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

void Component::addWire(const std::shared_ptr<WireBase>& wire)
{
    if (!wire) return;
    wire->setOwner(shared_from_this());
    all_wires.push_back(wire);
    if (auto single_bit = std::dynamic_pointer_cast<Wire<>>(wire)) {
        wires.push_back(single_bit);
    }
}

std::string Component::format(int lvl, bool formatWires, bool formatPins) const
{
    std::stringstream ss;
    std::string indent(lvl * 2, ' ');
    std::string type = getTypeName();

    ss << indent << "(" << type << ") '" << getID() << "'\n";

    if (formatWires)
    {
        for (const auto& wire : all_wires) if(wire)
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
