#pragma once

#include "components/ComponentBuilder.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include <utility>

template<typename T, typename... Args>
std::shared_ptr<T> ComponentBuilder::addNewComponent(std::string name, Args&&... args) {
    std::string scopedName = getScopedName(name);

    if (namedComponents.count(scopedName)) {
        return std::dynamic_pointer_cast<T>(namedComponents[scopedName]);
    }

    // Use the universal Component::create factory. It handles buildInternals and initPins.
    auto new_component = Component::create<T>(name, std::forward<Args>(args)...);

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

template<typename T>
std::shared_ptr<Pin> ComponentBuilder::getInputPin(const std::string& component_name, const std::string& pin_name) {
    auto comp = getComponent<IOComponent>(component_name);
    if (!comp) {
        return nullptr;
    }
    return comp->getInputPin(pin_name);
}

template<typename T>
std::shared_ptr<Pin> ComponentBuilder::getOutputPin(const std::string& component_name, const std::string& pin_name) {
    auto comp = getComponent<IOComponent>(component_name);
    if (!comp) {
        return nullptr;
    }
    return comp->getOutputPin(pin_name);
}