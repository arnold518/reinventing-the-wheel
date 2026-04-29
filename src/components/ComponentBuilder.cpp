#include "components/ComponentBuilder.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/WireBuilder.hpp"
#include "basic/Pin.hpp"
#include "basic/PinBase.hpp"
#include "basic/Wire.hpp"
#include "basic/WireBase.hpp"
#include <utility>
#include "components/ComponentBuilder.tpp"

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
    if (!root || root->getParent() == nullptr) {
        return name;
    }
    return root->getName() + "." + name;
}

std::shared_ptr<Wire<>> ComponentBuilder::addNewWire(
    std::string name,
    std::shared_ptr<Pin<>> source_pin,
    const std::vector<std::shared_ptr<Pin<>>>& sink_pins) {
    return addNewWire<1>(std::move(name), std::move(source_pin), sink_pins);
}

std::shared_ptr<WireBase> ComponentBuilder::addNewWireDynamic(
    std::string name,
    size_t width,
    std::shared_ptr<PinBase> source_pin,
    const std::vector<std::shared_ptr<PinBase>>& sink_pins) {
    auto build = [&]<size_t WIDTH>() -> std::shared_ptr<WireBase> {
        std::vector<std::shared_ptr<Pin<WIDTH>>> typed_sinks;
        typed_sinks.reserve(sink_pins.size());
        for (const auto& sink : sink_pins) {
            typed_sinks.push_back(std::dynamic_pointer_cast<Pin<WIDTH>>(sink));
        }
        return addNewWire<WIDTH>(
            std::move(name),
            std::dynamic_pointer_cast<Pin<WIDTH>>(source_pin),
            typed_sinks);
    };

    switch (width) {
        case 1: return build.operator()<1>();
        case 2: return build.operator()<2>();
        case 3: return build.operator()<3>();
        case 4: return build.operator()<4>();
        case 8: return build.operator()<8>();
        case 16: return build.operator()<16>();
        case 32: return build.operator()<32>();
        default: return nullptr;
    }
}

std::shared_ptr<Wire<>> ComponentBuilder::getWire(const std::string& name) {
    return std::dynamic_pointer_cast<Wire<>>(getWireDynamic(name));
}

std::shared_ptr<WireBase> ComponentBuilder::getWireDynamic(const std::string& name) {
    auto it = namedWires.find(getScopedName(name));
    if (it == namedWires.end()) {
        return nullptr;
    }
    return it->second;
}

std::shared_ptr<Pin<>> ComponentBuilder::getInputPin(const std::string& pin_name) {
    if (auto io_root = std::dynamic_pointer_cast<IOComponent>(getCurrentRoot())) {
        return io_root->getInputPin(pin_name);
    }
    return nullptr;
}

std::shared_ptr<Pin<>> ComponentBuilder::getOutputPin(const std::string& pin_name) {
    if (auto io_root = std::dynamic_pointer_cast<IOComponent>(getCurrentRoot())) {
        return io_root->getOutputPin(pin_name);
    }
    return nullptr;
}

WireBuilder ComponentBuilder::wire(std::string name) {
    return WireBuilder(this, std::move(name));
}
