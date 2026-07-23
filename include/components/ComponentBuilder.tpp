#pragma once

#include "components/ComponentBuilder.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "basic/Pin.hpp"
#include "basic/Wire.hpp"
#include "components/selection/BuildContext.hpp"
#include <utility>

template<typename T, typename... Args>
std::shared_ptr<T> ComponentBuilder::addNewComponent(std::string name, Args&&... args) {
    std::string scopedName = getScopedName(name);

    if (namedComponents.count(scopedName)) {
        return std::dynamic_pointer_cast<T>(namedComponents[scopedName]);
    }

    // Use the universal Component::create factory. It handles buildInternals and initPins.
    auto child_context = build_context_
        ? build_context_->child(name, std::nullopt)
        : nullptr;
    auto new_component = Component::createWithContext<T>(
        child_context, name, std::forward<Args>(args)...);

    if (new_component) {
        namedComponents[scopedName] = new_component;
        if(auto root = getCurrentRoot()) {
            root->addChild(new_component);
        }
    }
    return new_component;
}

template<typename T>
std::shared_ptr<T> ComponentBuilder::getComponent(const std::string& name) {
    std::string scopedName = getScopedName(name);
    auto it = namedComponents.find(scopedName);
    if (it == namedComponents.end()) {
        // Fallback for getting top-level components by their unscoped name
        it = namedComponents.find(name);
        if (it == namedComponents.end()) return nullptr;
    }
    return std::dynamic_pointer_cast<T>(it->second);
}

template<typename T, size_t WIDTH>
std::shared_ptr<Pin<WIDTH>> ComponentBuilder::getInputPin(const std::string& component_name, const std::string& pin_name) {
    auto comp = getComponent<IOComponent>(component_name);
    if (!comp) {
        return nullptr;
    }
    return comp->getInputPin<WIDTH>(pin_name);
}

template<typename T, size_t WIDTH>
std::shared_ptr<Pin<WIDTH>> ComponentBuilder::getOutputPin(const std::string& component_name, const std::string& pin_name) {
    auto comp = getComponent<IOComponent>(component_name);
    if (!comp) {
        return nullptr;
    }
    return comp->getOutputPin<WIDTH>(pin_name);
}

template<size_t WIDTH>
std::shared_ptr<Wire<WIDTH>> ComponentBuilder::addNewWire(
    std::string name,
    std::shared_ptr<Pin<WIDTH>> source_pin,
    const std::vector<std::shared_ptr<Pin<WIDTH>>>& sink_pins) {
    auto new_wire = std::make_shared<Wire<WIDTH>>(name);
    std::vector<std::shared_ptr<Component>> all_owners;

    if (source_pin) {
        new_wire->setSourcePin(source_pin);
        if (auto owner = source_pin->getOwner()) {
            all_owners.push_back(owner);
        }

        if (sink_pins.empty() && source_pin->getType() == PinType::OUTPUT) {
            source_pin->connectExternal(new_wire);
        }
    }

    for (const auto& sink_pin : sink_pins) {
        if (!sink_pin) {
            continue;
        }

        auto sink_owner = sink_pin->getOwner();
        if (sink_owner) {
            all_owners.push_back(sink_owner);
        }

        if (!source_pin) {
            sink_pin->connectExternal(new_wire);
            new_wire->addSinkPin(sink_pin);
            continue;
        }

        auto source_owner = source_pin->getOwner();
        if (!source_owner || !sink_owner) {
            continue;
        }

        if (source_owner == sink_owner) {
            if (source_pin->getType() == PinType::OUTPUT && sink_pin->getType() == PinType::INPUT) {
                source_pin->connectInternal(new_wire);
                sink_pin->connectInternal(new_wire);
                new_wire->addSinkPin(sink_pin);
            }
        } else if (source_owner->isAncestorOf(sink_owner)) {
            if (source_pin->getType() == PinType::INPUT && sink_pin->getType() == PinType::INPUT) {
                source_pin->connectInternal(new_wire);
                sink_pin->connectExternal(new_wire);
                new_wire->addSinkPin(sink_pin);
            }
        } else if (sink_owner->isAncestorOf(source_owner)) {
            if (source_pin->getType() == PinType::OUTPUT && sink_pin->getType() == PinType::OUTPUT) {
                source_pin->connectExternal(new_wire);
                sink_pin->connectInternal(new_wire);
                new_wire->addSinkPin(sink_pin);
            }
        } else {
            if (source_pin->getType() == PinType::OUTPUT && sink_pin->getType() == PinType::INPUT) {
                source_pin->connectExternal(new_wire);
                sink_pin->connectExternal(new_wire);
                new_wire->addSinkPin(sink_pin);
            }
        }
    }

    std::shared_ptr<Component> wire_owner;
    if (!all_owners.empty()) {
        wire_owner = Component::findLCA(all_owners);
    }
    if (!wire_owner) {
        wire_owner = getCurrentRoot();
    }
    if (wire_owner) {
        wire_owner->addWire(new_wire);
    }

    namedWires[getScopedName(name)] = new_wire;
    return new_wire;
}
