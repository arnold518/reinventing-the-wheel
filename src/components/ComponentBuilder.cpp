#include "components/ComponentBuilder.hpp"
#include <iostream>
#include <utility>
#include "basic/Wire.hpp"

ComponentBuilder::ComponentBuilder(std::shared_ptr<Component> ptr) : root(std::move(ptr)) {}

std::shared_ptr<Wire> ComponentBuilder::addNewWire(std::string name, std::shared_ptr<Pin> out, const std::vector<std::shared_ptr<Pin>>& ins) {
    if (name.empty()) {
        name = "Wire";
    }

    if (namedWires.count(name)) {
        std::string originalName = name;
        int counter = 1;
        do {
            name = originalName + "_" + std::to_string(counter++);
        } while (namedWires.count(name));
        std::cerr << "Warning: Wire with name '" << originalName << "' already exists. Renaming to '" << name << "'." << std::endl;
    }

    auto new_wire = std::make_shared<Wire>(name);
    if (new_wire) {
        namedWires[name] = new_wire;
        root->addWire(new_wire);
        if (out) out->connect(new_wire);
        for(const auto& in : ins) if (in) in->connect(new_wire);
    }
    return new_wire;
}

std::shared_ptr<Wire> ComponentBuilder::getWire(const std::string& name) {
    auto it = namedWires.find(name);
    if (it == namedWires.end()) {
        std::cerr << "Error: Wire with name '" << name << "' not found." << std::endl;
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<Pin> ComponentBuilder::getInputPin(const std::string& pin_name) {
    if (auto io_root = std::dynamic_pointer_cast<IOComponent>(root)) {
        return io_root->getInputPin(pin_name);
    }
    
    std::cerr << "Error: Cannot get input pin '" << pin_name << "' from root because root is not an IOComponent." << std::endl;
    return nullptr;
}

std::shared_ptr<Pin> ComponentBuilder::getOutputPin(const std::string& pin_name) {
    if (auto io_root = std::dynamic_pointer_cast<IOComponent>(root)) {
        return io_root->getOutputPin(pin_name);
    }

    std::cerr << "Error: Cannot get output pin '" << pin_name << "' from root because root is not an IOComponent." << std::endl;
    return nullptr;
}