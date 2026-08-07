#include "modules/rv32i/RV32IBuildProfiles.hpp"

#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/memory/Memory64Kx32.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32ISingleCycleSystem.hpp"
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
            circuit::ProfileSelector::exactPath(core_path + "REGISTER_FILE"),
            "use the verified register-file abstraction"),
    };
    for (const auto* child : {
             "CONTROL_FLOW", "DECODE_CONTROL", "ALU", "EXECUTION_STATUS"}) {
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

    for (const auto& contract : catalog.contracts()) {
        if (!contract.id.starts_with("rv32i.")) {
            continue;
        }
        builder.addRule(circuit::preferFidelity(
            circuit::Fidelity::Structural,
            circuit::ProfileSelector::contract(contract.id),
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
    for (const auto* family : {
             &circuit::families::Memory64Kx32,
             &circuit::families::RegisterFile32x32,
             &circuit::families::Register32,
             &circuit::families::MemoryBit,
         }) {
        overrides.push_back(circuit::preferFidelity(
            circuit::Fidelity::Behavioral,
            circuit::ProfileSelector::contract(*family),
            "use the verified behavioral storage visualization"));
    }
    return circuit::withProfileOverrides(
        std::move(base),
        std::move(overrides),
        std::move(profile_name));
}

} // namespace rv32i
