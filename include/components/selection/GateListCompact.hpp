#pragma once
#include "components/BasicComponent.hpp"
#include "components/PartDelays.hpp"
#include "components/selection/SelectionTypes.hpp"
#include <memory>
#include <set>
#include <string>

namespace circuit {
class ComponentCatalog;

// The compact form of a gate-level part that has no hand-written one: one
// leaf that evaluates the part's own gates, flattened once per kind of part
// (analysis/DelayTwin.hpp), and publishes the settled values on the delays
// proven on those gates. The values are the gates' by construction (four
// state; a bit wired straight from an input passes Z on unchanged). Only
// combinational parts of plain gates qualify (DelayTwin::exact_values), with
// any parameters their structural form supports.
// Selected only by an exact-path choice (the visualizer explorer, tests) or
// a parent's requirement, never by a general profile rule, so no existing
// profile changes what it builds.
class GateListCompact final : public BasicComponent {
public:
    GateListCompact(std::string name, std::string type, const std::vector<PinDescriptor>& pins,
                    std::shared_ptr<const DelayTwin> gates);
    const char* getTypeName() const override { return type_.c_str(); }
    void evaluate(SimulationTime current_time, Simulator& simulator) override;
    StateSupport stateSupport() const override { return StateSupport::Stateless; }
private:
    std::string type_;
    std::shared_ptr<const DelayTwin> gates_;
};

// The gate list of a contract's structural form with these parameters, or
// null when it has none with exact values. Built once per timing profile,
// contract and parameters, and shared.
std::shared_ptr<const DelayTwin> exactGateList(const std::string& contract_id, const ParameterMap& parameters = {});
// Its delays: the stored proven table when there is one, otherwise proven on
// the same gates now, with the generator's options (part_delay_tables).
const PartDelays& gateListDelays(const std::string& contract_id, const ParameterMap& parameters = {});
// Contracts that get a GateListCompact (GateListCompacts.generated.inc).
const std::set<std::string>& gateListCompactContracts();
// Registers it for each listed contract that has no compact form of its own.
void registerGateListCompacts(ComponentCatalog& catalog);
inline const char* gateListCompactEvidence() { return "evaluates the component's own gate list"; }
} // namespace circuit
