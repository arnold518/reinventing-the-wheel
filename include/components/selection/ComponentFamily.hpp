#pragma once

#include "components/selection/SelectionTypes.hpp"
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class IOComponent;
class Component;

namespace circuit {

class BuildContext;
using PinInitializer = std::function<void(IOComponent*)>;
using FamilyFactory = std::function<std::shared_ptr<Component>(
    const std::string&, const std::shared_ptr<BuildContext>&)>;

/**
 * Public identity of a replaceable component contract.
 *
 * A family is deliberately not a Component base class. It owns the shared
 * public identity, pin contract, and internal factories, while builders,
 * profiles, tests, bindings, and visualizers refer only to the family. The
 * catalog adds verification evidence and selection priority without exposing
 * implementation class names to callers.
 */
class ComponentFamily {
public:
    ComponentFamily(std::string_view contract_id,
                    std::string_view type_name,
                    PinInitializer pin_initializer = nullptr,
                    FamilyFactory structural_factory = nullptr,
                    FamilyFactory behavioral_factory = nullptr,
                    Fidelity default_fidelity = Fidelity::Structural)
        : contract_id_(contract_id),
          type_name_(type_name),
          pin_initializer_(std::move(pin_initializer)),
          structural_factory_(std::move(structural_factory)),
          behavioral_factory_(std::move(behavioral_factory)),
          default_fidelity_(default_fidelity) {}

    std::string_view id() const { return contract_id_; }
    std::string_view typeName() const { return type_name_; }
    const PinInitializer& pinInitializer() const {
        return pin_initializer_;
    }

    std::shared_ptr<Component> createDefault(
        const std::string& instance_name,
        const std::shared_ptr<BuildContext>& context) const {
        return create(default_fidelity_, instance_name, context);
    }

    bool supports(Fidelity fidelity) const {
        return static_cast<bool>(
            fidelity == Fidelity::Structural
                ? structural_factory_
                : behavioral_factory_);
    }

    std::shared_ptr<Component> create(
        Fidelity fidelity,
        const std::string& instance_name,
        const std::shared_ptr<BuildContext>& context) const {
        const auto& factory = fidelity == Fidelity::Structural
            ? structural_factory_
            : behavioral_factory_;
        if (!factory) {
            throw std::logic_error(
                "Component family does not provide requested fidelity");
        }
        return factory(instance_name, context);
    }

    ComponentBuildRequest request(
        std::string instance_name,
        ParameterMap parameters = {},
        std::vector<std::string> required_capabilities = {},
        std::string semantic_domain = {},
        std::string observation = {}) const {
        return {
            std::string(contract_id_),
            std::move(instance_name),
            std::move(parameters),
            std::move(required_capabilities),
            std::move(semantic_domain),
            std::move(observation),
        };
    }

private:
    std::string_view contract_id_;
    std::string_view type_name_;
    PinInitializer pin_initializer_;
    FamilyFactory structural_factory_;
    FamilyFactory behavioral_factory_;
    Fidelity default_fidelity_;
};

} // namespace circuit
