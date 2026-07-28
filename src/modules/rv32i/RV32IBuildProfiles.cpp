#include "modules/rv32i/RV32IBuildProfiles.hpp"

#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
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

    return circuit::withProfileOverrides(
        circuit::structuralThroughDepth(
            1,
            circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException),
        std::move(overrides),
        "rv32i-balanced");
}

} // namespace rv32i
