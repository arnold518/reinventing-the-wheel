#include "components/ComponentBuilder.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/WireBuilder.hpp"
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
    std::cout << "[BUILDER] Creating wire '" << getScopedName(name) << "'" << std::endl;
    
    auto new_wire = std::make_shared<Wire>(name);
    std::vector<std::shared_ptr<Component>> all_owners;

    // --- Handle source-less wires (GND/VCC) ---
    if (!source_pin) {
        std::cout << "[BUILDER] |-> Type: Source-less (GND/VCC)" << std::endl;
        for (const auto& sink_pin : sink_pins) {
            if (!sink_pin) continue;
            std::cout << "[BUILDER] |   - Connecting sink: '" << sink_pin->getID() << "' (External)" << std::endl;
            new_wire->addSinkPin(sink_pin);
            sink_pin->connectExternal(new_wire);
            all_owners.push_back(sink_pin->getOwner());
        }
    } else {
        // --- Handle wires with a source pin ---
        auto source_owner = source_pin->getOwner();
        all_owners.push_back(source_owner);
        new_wire->setSourcePin(source_pin);
        std::cout << "[BUILDER] |-> Source: '" << source_pin->getID() << "'" << std::endl;

        for (const auto& sink_pin : sink_pins) {
            if (!sink_pin) continue;
            auto sink_owner = sink_pin->getOwner();
            std::cout << "[BUILDER] |   - Analyzing connection to sink: '" << sink_pin->getID() << "'" << std::endl;

            // Determine the connection type based on the source-sink relationship
            
            // Rule: Self-Connection
            if (source_owner == sink_owner) {
                std::cout << "[BUILDER] |     - Detected: Self-Connection" << std::endl;
                if (source_pin->getType() == PinType::OUTPUT && sink_pin->getType() == PinType::INPUT) {
                    source_pin->connectInternal(new_wire); // CORRECTED: Self-connections are internal
                    sink_pin->connectInternal(new_wire);   // CORRECTED: Self-connections are internal
                    new_wire->addSinkPin(sink_pin);
                    all_owners.push_back(sink_owner);
                    std::cout << "[BUILDER] |     - Action: Connected source and sink internally." << std::endl;
                }
            // Rule: Hierarchical Downward (Parent -> Child)
            } else if (source_owner->isAncestorOf(sink_owner)) {
                std::cout << "[BUILDER] |     - Detected: Hierarchical Downward" << std::endl;
                if (source_pin->getType() == PinType::INPUT && sink_pin->getType() == PinType::INPUT) {
                    source_pin->connectInternal(new_wire);
                    sink_pin->connectExternal(new_wire);
                    new_wire->addSinkPin(sink_pin);
                    all_owners.push_back(sink_owner);
                    std::cout << "[BUILDER] |     - Action: Connected source (Internal) to sink (External)." << std::endl;
                }
            // Rule: Hierarchical Upward (Child -> Parent)
            } else if (sink_owner->isAncestorOf(source_owner)) {
                std::cout << "[BUILDER] |     - Detected: Hierarchical Upward" << std::endl;
                if (source_pin->getType() == PinType::OUTPUT && sink_pin->getType() == PinType::OUTPUT) {
                    source_pin->connectExternal(new_wire);
                    sink_pin->connectInternal(new_wire);
                    new_wire->addSinkPin(sink_pin);
                    all_owners.push_back(sink_owner);
                    std::cout << "[BUILDER] |     - Action: Connected source (External) to sink (Internal)." << std::endl;
                }
            // Rule: Peer-to-Peer
            } else {
                std::cout << "[BUILDER] |     - Detected: Peer-to-Peer" << std::endl;
                if (source_pin->getType() == PinType::OUTPUT && sink_pin->getType() == PinType::INPUT) {
                    source_pin->connectExternal(new_wire);
                    sink_pin->connectExternal(new_wire);
                    new_wire->addSinkPin(sink_pin);
                    all_owners.push_back(sink_owner);
                    std::cout << "[BUILDER] |     - Action: Connected source and sink externally." << std::endl;
                }
            }
        }
    }

    // --- Determine Ownership and Finalize ---
    if (all_owners.empty()) {
        std::cerr << "Warning: Wire '" << name << "' has no connections." << std::endl;
        namedWires[getScopedName(name)] = new_wire;
        return new_wire;
    }
    
    auto wire_owner = Component::findLCA(all_owners);
    if (wire_owner) {
        wire_owner->addWire(new_wire);
        std::cout << "[BUILDER] |-> Final Owner: '" << wire_owner->getID() << "'" << std::endl;
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

WireBuilder ComponentBuilder::wire(std::string name) {
    return WireBuilder(this, std::move(name));
}