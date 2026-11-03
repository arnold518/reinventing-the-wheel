#pragma once
// Registers a system-level contract whose pins depend on its parameters (a
// bus, a router, a memory subsystem, a whole computer). The parameters are
// parsed into a configuration once per use; an invalid set is unsupported.
#include "components/selection/ComponentCatalog.hpp"
#include "components/IOComponent.hpp"
#include <functional>

namespace circuit::system {

// The structural circuit a compiled (gate-list) form runs: the contract built
// all-structural except storage, which uses its characterized compact forms.
std::shared_ptr<IOComponent> characterizedReference(const ComponentBuildRequest&);

template <class Config>
struct SystemPart {
    std::string id, type, test;
    std::function<Config(const ParameterMap&)> parse;
    std::function<IOComponent::PinInitFunction(const Config&)> pins;
    std::function<std::shared_ptr<Component>(const std::string&, const Config&, const std::shared_ptr<BuildContext>&)> structural;
    std::function<std::shared_ptr<Component>(const std::string&, const Config&, const std::shared_ptr<BuildContext>&)> behavioral = {};
    std::vector<std::string> capabilities = {};
    std::string observation = "clock-boundary";
    std::string equivalence_test = {};
    // The compact form is a compiled gate list chosen only by an exact-path
    // profile rule or a parent's requirement, never by a general preference.
    bool behavioral_on_request = false;
};

template <class Config>
void registerSystemPart(ComponentCatalog& catalog, SystemPart<Config> part) {
    const auto schema = [parse = part.parse, pins = part.pins](const ParameterMap& parameters) {
        auto io = Component::create<IOComponent>("SCHEMA", pins(parse(parameters)));
        std::vector<PinDescriptor> result;
        for (const auto& [name, pin] : io->getAllInputPins()) result.push_back({name, pin->getWidth(), PinType::INPUT});
        for (const auto& [name, pin] : io->getAllOutputPins()) result.push_back({name, pin->getWidth(), PinType::OUTPUT});
        return result;
    };
    ContractDescriptor contract{part.id, 1, part.type, {}, part.capabilities, "four-state", part.observation, schema};
    try { contract.pins = schema({}); } catch (const std::exception&) {}
    catalog.registerContract(std::move(contract));
    const auto supports = [parse = part.parse](const ParameterMap& parameters) {
        try { (void)parse(parameters); return true; } catch (const std::exception&) { return false; }
    };
    for (const bool behavioral : {false, true}) {
        const auto& make = behavioral ? part.behavioral : part.structural;
        if (!make) continue;
        ImplementationDescriptor d;
        d.id = part.id + (behavioral ? ".behavioral" : ".structural");
        d.contract_id = part.id;
        d.concrete_type_name = part.type;
        d.fidelity = behavioral ? Fidelity::Behavioral : Fidelity::Structural;
        d.gate_list = behavioral && part.behavioral_on_request;
        d.capabilities = part.capabilities;
        d.supports = supports;
        d.factory = [make, parse = part.parse](const std::string& name, const ParameterMap& parameters,
                                               const std::shared_ptr<BuildContext>& context) {
            return make(name, parse(parameters), context);
        };
        d.evidence = {VerificationStatus::Characterized,
                      {behavioral ? "the part's own gates, compiled to a gate list" : "composition of tested parts"},
                      {part.test},
                      behavioral && !part.equivalence_test.empty() ? std::vector<std::string>{part.equivalence_test} : std::vector<std::string>{},
                      {}, "four-state", part.observation};
        catalog.registerImplementation(std::move(d));
    }
}

} // namespace circuit::system
