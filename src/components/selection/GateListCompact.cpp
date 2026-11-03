#include "components/selection/GateListCompact.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/Component.tpp"
#include "analysis/DelayTwin.hpp"
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/ProvenPartDelays.hpp"
#include <map>
#include <mutex>

namespace circuit {
namespace {
void addPins(IOComponent* self, const std::vector<PinDescriptor>& pins) {
    for (const auto& pin : pins) self->addPinDynamic(pin.name, pin.direction, pin.width);
}
}

GateListCompact::GateListCompact(std::string name, std::string type, const std::vector<PinDescriptor>& pins,
                                 std::shared_ptr<const DelayTwin> gates)
    : BasicComponent(std::move(name), 1, [pins](IOComponent* self) { addPins(self, pins); }),
      type_(std::move(type)), gates_(std::move(gates)) {}

void GateListCompact::evaluate(SimulationTime current_time, Simulator& simulator) {
    const auto& gates = *gates_;
    std::vector<std::vector<LogicValue>> values;
    std::vector<std::vector<long long>> changed;
    values.reserve(gates.inputs.size());
    for (const auto& [name, width] : gates.inputs) {
        values.push_back(getInputPinDynamic(name)->getValueAsVector());
        changed.emplace_back(width, static_cast<long long>(current_time));
    }
    std::vector<long long> settled;
    std::vector<std::vector<uint8_t>> result;
    gates.settle(nullptr, static_cast<long long>(current_time), values, changed, settled, result);
    for (size_t pin = 0; pin < gates.outputs.size(); ++pin) {
        std::vector<LogicValue> out(gates.outputs[pin].second);
        for (size_t bit = 0; bit < out.size(); ++bit) {
            const auto [from, from_bit] = gates.wired[pin][bit];
            out[bit] = from >= 0 ? values[from][from_bit]
                     : result[pin][bit] == 0 ? LogicValue::LOW
                     : result[pin][bit] == 1 ? LogicValue::HIGH : LogicValue::UNKNOWN;
        }
        _updateOutputWireDynamic(simulator, gates.outputs[pin].first, out, current_time);
    }
}

namespace {
std::string partKey(const std::string& contract_id, const ParameterMap& parameters) {
    std::string key = timing::activeTimingProfile().name + "|" + contract_id;
    for (const auto& [name, value] : parameters) key += "|" + name + "=" + value;
    return key;
}
std::shared_ptr<Component> gateLevel(const std::string& contract_id, const ParameterMap& parameters) {
    return builtinComponentCatalog().createRoot({contract_id, "GATES", parameters}, maximallyStructural()).root;
}
}

std::shared_ptr<const DelayTwin> exactGateList(const std::string& contract_id, const ParameterMap& parameters) {
    static std::mutex guard;
    static std::map<std::string, std::shared_ptr<const DelayTwin>> lists;
    const auto key = partKey(contract_id, parameters);
    {
        const std::lock_guard<std::mutex> lock(guard);
        if (const auto found = lists.find(key); found != lists.end()) return found->second;
    }
    std::shared_ptr<const DelayTwin> list;
    try {
        const auto gates = gateLevel(contract_id, parameters);
        if (gates && !std::dynamic_pointer_cast<BasicComponent>(gates))
            if (auto twin = analysis::buildDelayTwin(gates); twin && twin->exact_values) list = std::move(twin);
    } catch (const std::exception&) {}
    const std::lock_guard<std::mutex> lock(guard);
    return lists.emplace(key, list).first->second;
}

const PartDelays& gateListDelays(const std::string& contract_id, const ParameterMap& parameters) {
    static std::mutex guard;
    static std::map<std::string, PartDelays> tables;
    const auto key = partKey(contract_id, parameters);
    {
        const std::lock_guard<std::mutex> lock(guard);
        if (const auto found = tables.find(key); found != tables.end()) return found->second;
    }
    PartDelays table;
    if (const auto* stored = provenPartDelays(contract_id + ".gate-list", parameters)) table = *stored;
    else {
        analysis::SettlingOptions options;
        options.search_budget = 4000;
        if (const auto gates = std::dynamic_pointer_cast<IOComponent>(gateLevel(contract_id, parameters))) {
            if (gates->getInputPinDynamic("RST")) options.fixed_inputs["RST"] = 0;
            table = analysis::CircuitTimingAnalyzer::delays(gates, options);
            table.evidence = "proven on " + contract_id + ".structural when first built";
        }
    }
    const std::lock_guard<std::mutex> lock(guard);
    return tables.emplace(key, std::move(table)).first->second;
}

const std::set<std::string>& gateListCompactContracts() {
    static const std::set<std::string> generated = {
#include "GateListCompacts.generated.inc"
    };
    return generated;
}

void registerGateListCompacts(ComponentCatalog& catalog) {
    for (const auto& id : gateListCompactContracts()) {
        if (!catalog.hasContract(id)) continue;
        bool compact = false;
        for (const auto& implementation : catalog.implementationsFor(id))
            compact = compact || implementation.fidelity == Fidelity::Behavioral;
        if (compact) continue;
        const auto& contract = catalog.contract(id);
        ImplementationDescriptor d;
        d.id = id + ".gate-list";
        d.contract_id = id;
        d.fidelity = Fidelity::Behavioral;
        d.gate_list = true;
        d.concrete_type_name = contract.display_name;
        for (const auto& implementation : catalog.implementationsFor(id))
            if (implementation.fidelity == Fidelity::Structural) d.concrete_type_name = implementation.concrete_type_name;
        // The contract keeps its one logical owner (the gate-level form's
        // contract test); the compiled form adds its equivalence evidence.
        std::vector<std::string> owners;
        for (const auto& implementation : catalog.implementationsFor(id))
            if (implementation.fidelity == Fidelity::Structural) owners = implementation.evidence.contract_tests;
        d.evidence = {VerificationStatus::Characterized, {gateListCompactEvidence()},
                      owners, {"GateListCompactEquivalenceTest"}, {},
                      contract.semantic_domain, contract.observation};
        // Every parameter set the gates support; whether its gate list is
        // exact is checked when the part is built.
        for (const auto& implementation : catalog.implementationsFor(id))
            if (implementation.fidelity == Fidelity::Structural && !d.supports) d.supports = implementation.supports;
        if (!d.supports) d.supports = [](const ParameterMap& parameters) { return parameters.empty(); };
        const auto type = d.concrete_type_name;
        const auto& described = contract;
        d.factory = [id, type, described](const std::string& name, const ParameterMap& parameters,
                                          const std::shared_ptr<BuildContext>& context) -> std::shared_ptr<Component> {
            auto gates = exactGateList(id, parameters);
            if (!gates) throw std::runtime_error("No exact gate list for " + id);
            auto part = Component::createWithContext<GateListCompact>(context, name, type, described.pinsFor(parameters), gates);
            auto table = gateListDelays(id, parameters);
            table.twin = gates;
            part->setDelays(std::move(table));
            return part;
        };
        catalog.registerImplementation(std::move(d));
    }
}
} // namespace circuit
