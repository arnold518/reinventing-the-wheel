#pragma once

#include <memory>
#include <string>
#include <map>
#include <vector>
#include "ForwardDeclarations.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "components/selection/SelectionTypes.hpp"

// Forward declaration for WireBuilder
class WireBuilder;

class ComponentBuilder
{
private:
    std::vector<std::shared_ptr<Component>> contextStack;
    std::map<std::string, std::shared_ptr<Component>> namedComponents;
    std::map<std::string, std::shared_ptr<WireBase>> namedWires;
    std::shared_ptr<circuit::BuildContext> build_context_;

    std::shared_ptr<Component> getCurrentRoot();
    std::string getScopedName(const std::string& name);

public:
    explicit ComponentBuilder(
        std::shared_ptr<Component> ptr,
        std::shared_ptr<circuit::BuildContext> build_context = nullptr);

    // Template Declarations
    template<typename T, typename... Args>
    std::shared_ptr<T> addNewComponent(std::string name, Args&&... args);

    template<typename T>
    std::shared_ptr<T> getComponent(const std::string& name);
    
    template<typename T, size_t WIDTH = 1>
    std::shared_ptr<Pin<WIDTH>> getInputPin(const std::string& component_name, const std::string& pin_name);

    template<typename T, size_t WIDTH = 1>
    std::shared_ptr<Pin<WIDTH>> getOutputPin(const std::string& component_name, const std::string& pin_name);

    template<size_t WIDTH = 1>
    std::shared_ptr<Wire<WIDTH>> addNewWire(std::string name, std::shared_ptr<Pin<WIDTH>> out,
                                            const std::vector<std::shared_ptr<Pin<WIDTH>>>& ins = {});

    // Non-Template Declarations
    std::shared_ptr<Wire<>> addNewWire(std::string name, std::shared_ptr<Pin<>> out = nullptr,
                                       const std::vector<std::shared_ptr<Pin<>>>& ins = {});
    std::shared_ptr<WireBase> addNewWireDynamic(std::string name, size_t width, std::shared_ptr<PinBase> out,
                                                const std::vector<std::shared_ptr<PinBase>>& ins = {});
    std::shared_ptr<Wire<>> getWire(const std::string& name);
    std::shared_ptr<WireBase> getWireDynamic(const std::string& name);
    std::shared_ptr<Pin<>> getInputPin(const std::string& pin_name);
    std::shared_ptr<Pin<>> getOutputPin(const std::string& pin_name);

    std::shared_ptr<IOComponent> addByContract(
        circuit::ComponentBuildRequest request);

    std::shared_ptr<IOComponent> add(
        const circuit::ComponentFamily& family,
        std::string instance_name,
        circuit::ParameterMap parameters = {},
        std::vector<std::string> required_capabilities = {},
        std::string semantic_domain = {},
        std::string observation = {});

    const std::shared_ptr<circuit::BuildContext>& getBuildContext() const;

    // Wire Builder API (fluent interface)
    WireBuilder wire(std::string name);
};

// DO NOT INCLUDE THE .tpp FILE HERE. THIS BREAKS THE CYCLE.
// #include "components/ComponentBuilder.tpp"
