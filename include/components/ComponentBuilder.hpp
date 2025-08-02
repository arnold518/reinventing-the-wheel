#pragma once
#include <memory>
#include <string>
#include <map>
#include <vector>
#include "ForwardDeclarations.hpp"

class ComponentBuilder
{
private:
    std::shared_ptr<Component> root;
    std::map<std::string, std::shared_ptr<Component>> namedComponents;
    std::map<std::string, std::shared_ptr<Wire>> namedWires;

public:
    explicit ComponentBuilder(std::shared_ptr<Component> ptr);

    // --- Component and Wire Creation ---

    template<typename T, typename... Args>
    std::shared_ptr<T> addNewComponent(std::string name, Args&&... args);

    std::shared_ptr<Wire> addNewWire(std::string name, std::shared_ptr<Pin> out = nullptr, const std::vector<std::shared_ptr<Pin>>& ins = {});

    // --- Getters for Components and Wires ---

    template<typename T>
    std::shared_ptr<T> getComponent(const std::string& name);

    std::shared_ptr<Wire> getWire(const std::string& name);

    // --- Pin Getter Helpers ---

    template<typename T>
    std::shared_ptr<Pin> getInputPin(const std::string& component_name, const std::string& pin_name);

    template<typename T>
    std::shared_ptr<Pin> getOutputPin(const std::string& component_name, const std::string& pin_name);
};

#include "ComponentBuilder.tpp"