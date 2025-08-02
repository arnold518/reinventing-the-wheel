#include <type_traits>
#include "IOComponent.hpp"

template<typename T, typename... Args>
std::shared_ptr<T> ComponentBuilder::addNewComponent(std::string name, Args&&... args) {
    if (name.empty()) {
        if constexpr (requires { T::TypeName; }) {
            name = T::TypeName;
        } else {
            name = "Component";
        }
    }

    if (namedComponents.count(name)) {
        std::string originalName = name;
        int counter = 1;
        do {
            name = originalName + "_" + std::to_string(counter++);
        } while (namedComponents.count(name));
        std::cerr << "Warning: Component with name '" << originalName << "' already exists. Renaming to '" << name << "'." << std::endl;
    }
    
    std::shared_ptr<T> new_component;
    if constexpr (std::is_base_of_v<IOComponent, T>) {
        new_component = IOComponent::create<T>(name, std::forward<Args>(args)...);
    } else {
        new_component = std::make_shared<T>(name, std::forward<Args>(args)...);
    }

    if (new_component) {
        namedComponents[name] = new_component;
        root->addChild(new_component);
    }
    return new_component;
}

template<typename T>
std::shared_ptr<T> ComponentBuilder::getComponent(const std::string& name) {
    auto it = namedComponents.find(name);
    if (it == namedComponents.end()) {
        std::cerr << "Error: Component with name '" << name << "' not found." << std::endl;
        return nullptr;
    }
    auto ptr = std::dynamic_pointer_cast<T>(it->second);
    if (!ptr) {
        std::cerr << "Error: Component '" << name << "' exists but is not of the requested type." << std::endl;
        return nullptr;
    }
    return ptr;
}

template<typename T>
std::shared_ptr<Pin> ComponentBuilder::getInputPin(const std::string& component_name, const std::string& pin_name) {
    auto comp = getComponent<T>(component_name);
    if (!comp) {
        return nullptr;
    }
    return comp->getInputPin(pin_name);
}

template<typename T>
std::shared_ptr<Pin> ComponentBuilder::getOutputPin(const std::string& component_name, const std::string& pin_name) {
    auto comp = getComponent<T>(component_name);
    if (!comp) {
        return nullptr;
    }
    return comp->getOutputPin(pin_name);
}