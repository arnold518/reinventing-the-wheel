#pragma once

#include "components/selection/StandardProfiles.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"

namespace circuit::test {
inline circuit::BuildProfile rv32imInspectionProfile() {
    // Match the existing core previews: expose architecture and functional
    // blocks while retaining compact storage and lower-level word parts.
    const auto& catalog = circuit::builtinComponentCatalog();
    auto profile = rv32i::architectureStructuralProfile(catalog, "rv32im-inspection");
    std::vector<circuit::ProfileRule> visible;
    for (const auto& contract : catalog.contracts()) {
        if (contract.id.starts_with("rv32im.") || contract.id.starts_with("arithmetic.") ||
            contract.id.starts_with("device.") || contract.id.starts_with("bus.") ||
            contract.id.starts_with("video.")) {
            visible.push_back(circuit::preferFidelity(circuit::Fidelity::Structural,
                circuit::ProfileSelector::contract(contract.id), "show the recorded component structure"));
        }
    }
    return circuit::withProfileOverrides(std::move(profile), std::move(visible), "rv32im-inspection");
}

} // namespace circuit::test
