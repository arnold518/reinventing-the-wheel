#include "components/ComponentBuilder.hpp"
#include "components/Component.hpp"
#include "components/IOComponent.hpp"
#include "components/WireBuilder.hpp"
#include "basic/Pin.hpp"
#include "basic/PinBase.hpp"
#include "basic/Wire.hpp"
#include "basic/WireBase.hpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include <stdexcept>
#include <utility>
#include "components/ComponentBuilder.tpp"

ComponentBuilder::ComponentBuilder(
    std::shared_ptr<Component> ptr,
    std::shared_ptr<circuit::BuildContext> build_context)
    : build_context_(std::move(build_context)) {
    if (ptr) {
        contextStack.push_back(std::move(ptr));
    }
}

std::shared_ptr<IOComponent> ComponentBuilder::addByContract(
    circuit::ComponentBuildRequest request) {
    if (!build_context_) {
        throw std::runtime_error("Contract-based construction requires a BuildContext");
    }
    if (request.instance_name.empty()) {
        throw std::invalid_argument("Contract build request requires an instance name");
    }
    const std::string scoped_name = getScopedName(request.instance_name);
    if (namedComponents.count(scoped_name)) {
        return std::dynamic_pointer_cast<IOComponent>(namedComponents[scoped_name]);
    }

    auto component = build_context_->catalog().createChild(request, build_context_);
    auto io_component = std::dynamic_pointer_cast<IOComponent>(component);
    if (!io_component) {
        throw std::runtime_error("Contract implementation did not create an IOComponent");
    }
    namedComponents[scoped_name] = component;
    if (auto root = getCurrentRoot()) {
        root->addChild(component);
    }
    return io_component;
}

std::shared_ptr<IOComponent> ComponentBuilder::add(
    const circuit::ComponentFamily& family,
    std::string instance_name,
    circuit::ParameterMap parameters,
    std::vector<std::string> required_capabilities,
    std::string semantic_domain,
    std::string observation) {
    if (!build_context_) {
        const std::string scoped_name = getScopedName(instance_name);
        if (namedComponents.count(scoped_name)) {
            return std::dynamic_pointer_cast<IOComponent>(
                namedComponents[scoped_name]);
        }
        const auto fidelity = family.supports(circuit::Fidelity::Structural)
            ? circuit::Fidelity::Structural
            : circuit::Fidelity::Behavioral;
        auto component = family.create(fidelity, instance_name, nullptr);
        auto io_component = std::dynamic_pointer_cast<IOComponent>(component);
        if (!io_component) {
            throw std::runtime_error(
                "Component family default did not create an IOComponent");
        }
        namedComponents[scoped_name] = component;
        if (auto root = getCurrentRoot()) {
            root->addChild(component);
        }
        return io_component;
    }
    return addByContract(family.request(
        std::move(instance_name),
        std::move(parameters),
        std::move(required_capabilities),
        std::move(semantic_domain),
        std::move(observation)));
}

const std::shared_ptr<circuit::BuildContext>& ComponentBuilder::getBuildContext() const {
    return build_context_;
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
        case 5: return build.operator()<5>();
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
