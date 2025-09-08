#include "components/ComponentBuilder.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "basic/Wire.hpp"
#include <utility>
#include <iostream>

ComponentBuilder::ComponentBuilder(std::shared_ptr<Component> ptr) {
    if (ptr) {
        contextStack.push_back(std::move(ptr));
    }
}

std::shared_ptr<Component> ComponentBuilder::getCurrentRoot() {
    if (contextStack.empty()) {
        return nullptr;
    }
    return contextStack.back();
}

std::string ComponentBuilder::getScopedName(const std::string& name) {
    auto root = getCurrentRoot();
    if (!root || root->getParent() == nullptr) { // At the top-level
        return name;
    }
    return root->getName() + "." + name;
}

std::shared_ptr<Wire> ComponentBuilder::addNewWire(std::string name, std::shared_ptr<Pin> source_pin, const std::vector<std::shared_ptr<Pin>>& sink_pins) {
    auto new_wire = std::make_shared<Wire>(name);
    std::vector<std::shared_ptr<Component>> all_owners;

    // --- Handle the source pin if it exists ---
    if (source_pin) {
        auto source_owner = source_pin->getOwner();
        all_owners.push_back(source_owner);

        new_wire->setSourcePin(source_pin);
        source_pin->connect(new_wire);

        // --- Validate each sink against the source ---
        for (const auto& sink_pin : sink_pins) {
            if (!sink_pin) continue;
            auto sink_owner = sink_pin->getOwner();
            bool connection_valid = false;

            if (source_owner == sink_owner) { // Self-connection
                if (source_pin->getType() == PinType::OUTPUT && sink_pin->getType() == PinType::INPUT) {
                    connection_valid = true;
                } else {
                    std::cerr << "Error: Invalid self-connection on '" << source_owner->getName() << "'. Must be OUTPUT to INPUT." << std::endl;
                }
            } else if (source_owner->isAncestorOf(sink_owner) || sink_owner->isAncestorOf(source_owner)) { // Hierarchical
                if (source_pin->getType() == sink_pin->getType()) {
                    connection_valid = true;
                } else {
                    std::cerr << "Error: Invalid hierarchical connection between '" << source_owner->getName() << "' and '" << sink_owner->getName() << "'. Pin types must match." << std::endl;
                }
            } else { // Peer-to-peer
                if (source_pin->getType() == PinType::OUTPUT && sink_pin->getType() == PinType::INPUT) {
                    connection_valid = true;
                } else {
                    std::cerr << "Error: Invalid peer connection between '" << source_owner->getName() << "' and '" << sink_owner->getName() << "'. Must be OUTPUT to INPUT." << std::endl;
                }
            }

            if (connection_valid) {
                new_wire->addSinkPin(sink_pin);
                sink_pin->connect(new_wire);
                all_owners.push_back(sink_owner);
            }
        }
    } else {
        // --- Handle the source-less case (e.g., GND/VCC) ---
        // No source validation is needed. Just connect the sinks.
        for (const auto& sink_pin : sink_pins) {
            if (!sink_pin) continue;
            new_wire->addSinkPin(sink_pin);
            sink_pin->connect(new_wire);
            all_owners.push_back(sink_pin->getOwner());
        }
    }

    // --- Determine Ownership and Finalize ---
    if (all_owners.empty()) {
        std::cerr << "Error: Wire '" << name << "' has no connections and cannot be owned. Please connect it to at least one pin." << std::endl;
        return nullptr;
    }
    
    auto wire_owner = Component::findLCA(all_owners);
    if (wire_owner) {
        wire_owner->addWire(new_wire);
    } else {
        std::cerr << "Error: Could not determine a common owner for wire '" << name << "'." << std::endl;
        return nullptr;
    }
    
    namedWires[getScopedName(name)] = new_wire;
    return new_wire;
}

std::shared_ptr<Wire> ComponentBuilder::getWire(const std::string& name) {
    std::string scopedName = getScopedName(name);
    auto it = namedWires.find(scopedName);
    if (it == namedWires.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<Pin> ComponentBuilder::getInputPin(const std::string& pin_name) {
    if (auto io_root = std::dynamic_pointer_cast<IOComponent>(getCurrentRoot())) {
        return io_root->getInputPin(pin_name);
    }
    return nullptr;
}

std::shared_ptr<Pin> ComponentBuilder::getOutputPin(const std::string& pin_name) {
    if (auto io_root = std::dynamic_pointer_cast<IOComponent>(getCurrentRoot())) {
        return io_root->getOutputPin(pin_name);
    }
    return nullptr;
}