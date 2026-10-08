#include "modules/rv32i/RV32IBuildProfiles.hpp"

#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/Register.hpp"
#include "modules/rv32i/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32ISingleCycleSystem.hpp"
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace rv32i {

circuit::ComponentBuildRequest educationalSystemRequest(std::string instance_name) {
    return circuit::families::RV32ISingleCycleSystem.request(
        std::move(instance_name));
}

circuit::BuildProfile balancedSystemProfile(
    const circuit::ComponentCatalog& catalog,
    const circuit::ComponentBuildRequest& root) {
    (void)catalog;
    const std::string core_path = root.instance_name + ".CORE.";
    std::vector<circuit::ProfileRule> overrides{
        circuit::preferFidelity(
            circuit::Fidelity::Behavioral,
            circuit::ProfileSelector::exactPath(root.instance_name + ".INSTRUCTION_MEMORY"),
            "use the verified large-memory abstraction"),
        circuit::preferFidelity(
            circuit::Fidelity::Behavioral,
            circuit::ProfileSelector::exactPath(root.instance_name + ".DATA_MEMORY"),
            "use the verified large-memory abstraction"),
        circuit::preferFidelity(
            circuit::Fidelity::Behavioral,
            circuit::ProfileSelector::exactPath(core_path + "DECODE.REGISTER_FILE"),
            "use the verified register-file abstraction"),
    };
    for (const auto* child : {
             "FETCH", "DECODE", "EXECUTE", "MEMORY", "WRITEBACK", "NEXT_PC", "EXECUTION_STATUS"}) {
        overrides.push_back(circuit::preferFidelity(
            circuit::Fidelity::Structural,
            circuit::ProfileSelector::subtree(core_path + child),
            "preserve the verified single-cycle timing boundary"));
    }

    return withBehavioralMemoryParts(
        circuit::withProfileOverrides(
            circuit::structuralThroughDepth(
                1,
                circuit::UnavailableFidelityPolicy::
                    UseOnlyAvailableAndRecordException),
            std::move(overrides),
            "rv32i-balanced"),
        "rv32i-balanced");
}

circuit::BuildProfile architectureStructuralProfile(
    const circuit::ComponentCatalog& catalog,
    std::string profile_name) {
    const auto resolved_name = profile_name.empty()
        ? std::string("rv32i-architecture-structural")
        : profile_name;
    auto builder = circuit::BuildProfileBuilder(
        resolved_name);
    builder.addRule(circuit::preferFidelity(
        circuit::Fidelity::Behavioral,
        circuit::ProfileSelector::any(),
        "keep non-RV32I implementation details compact"));

    // Saved layouts and profile overrides are keyed by this profile's
    // fingerprint, which covers its rule list. Retired RV32I contracts keep
    // their (now inert) rule, in the catalog's sorted order, so that removing
    // a component does not orphan every saved RV32I layout.
    std::set<std::string> contract_ids{"rv32i.pipeline.control-flow", "rv32i.control-flow"};
    for (const auto& contract : catalog.contracts()) {
        if (contract.id.starts_with("rv32i.")) {
            contract_ids.insert(contract.id);
        }
    }
    for (const auto& id : contract_ids) {
        builder.addRule(circuit::preferFidelity(
            circuit::Fidelity::Structural,
            circuit::ProfileSelector::contract(id),
            "show the RV32I architecture structurally"));
    }

    builder.unavailablePolicy(
        circuit::UnavailableFidelityPolicy::
            UseOnlyAvailableAndRecordException);
    return withBehavioralMemoryParts(
        builder.build(),
        resolved_name);
}

circuit::BuildProfile withBehavioralMemoryParts(
    circuit::BuildProfile base,
    std::string profile_name) {
    if (profile_name.empty()) {
        profile_name = base.name();
    }
    std::vector<circuit::ProfileRule> overrides;
    // Saved layouts are keyed by this profile's fingerprint, which covers the
    // rule text. The storage parts are therefore named by their retired IDs;
    // canonicalContractId() makes the rules match the current contracts.
    for (const std::string id : {
             std::string(circuit::families::RegisterFile32x32.id()),
             std::string("memory.write-enabled-bit"),
             std::string("memory.clocked-state-bit"),
         }) {
        overrides.push_back(circuit::preferFidelity(
            circuit::Fidelity::Behavioral,
            circuit::ProfileSelector::contract(id),
             "use the verified behavioral storage visualization"));
    }
    for (size_t width : {1,2,3,4,5,8,16,32})
        overrides.push_back(circuit::preferFidelity(
            circuit::Fidelity::Behavioral,
            circuit::ProfileSelector::contract("memory.register." + std::to_string(width)),
            "use a compact request or response payload word"));
    return circuit::withProfileOverrides(
        std::move(base),
        std::move(overrides),
        std::move(profile_name));
}

} // namespace rv32i
