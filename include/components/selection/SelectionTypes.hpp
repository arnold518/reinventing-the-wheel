#pragma once

#include "basic/PinBase.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace circuit {

enum class Fidelity {
    Structural,
    Behavioral,
};

enum class VerificationStatus {
    Unverified,
    Characterized,
    Verified,
};

enum class UnavailableFidelityPolicy {
    Error,
    UseOnlyAvailableAndRecordException,
};

using ParameterMap = std::map<std::string, std::string>;

struct PinDescriptor {
    std::string name;
    size_t width = 1;
    PinType direction = PinType::INPUT;

    bool operator==(const PinDescriptor&) const = default;
};

struct ContractDescriptor {
    std::string id;
    uint32_t version = 1;
    std::string display_name;
    std::vector<PinDescriptor> pins;
    std::vector<std::string> required_capabilities;
    std::string semantic_domain = "four-state";
    std::string observation = "stable-after-settle";
};

struct VerificationEvidence {
    VerificationStatus status = VerificationStatus::Unverified;
    std::vector<std::string> lower_level_evidence;
    std::vector<std::string> contract_tests;
    std::vector<std::string> equivalence_tests;
    std::vector<std::string> representative_tests;
    std::string semantic_domain = "unverified";
    std::string observation = "unverified";
};

struct ComponentBuildRequest {
    std::string contract_id;
    std::string instance_name;
    ParameterMap parameters;
    std::vector<std::string> required_capabilities;
    std::string semantic_domain;
    std::string observation;
};

struct ResolvedSelection {
    std::string contract_id;
    uint32_t contract_version = 1;
    std::string implementation_id;
    Fidelity fidelity = Fidelity::Structural;
    std::vector<Fidelity> available_fidelities;
    bool terminal_primitive = false;
    bool used_unavailable_exception = false;
    std::string selection_reason;
};

struct ComponentInstanceMetadata {
    std::string contract_id;
    uint32_t contract_version = 1;
    std::string implementation_id;
    Fidelity fidelity = Fidelity::Structural;
    std::vector<Fidelity> available_fidelities;
    bool terminal_primitive = false;
    bool used_unavailable_exception = false;
    std::string selection_reason;
    std::string profile_fingerprint;
};

std::string toString(Fidelity value);
std::string toString(VerificationStatus value);
std::string toString(UnavailableFidelityPolicy value);
std::string canonicalizeParameters(const ParameterMap& parameters);

} // namespace circuit
