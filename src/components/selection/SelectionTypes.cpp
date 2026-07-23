#include "components/selection/SelectionTypes.hpp"
#include <sstream>

namespace circuit {

std::string toString(Fidelity value) {
    switch (value) {
        case Fidelity::Structural: return "structural";
        case Fidelity::Behavioral: return "behavioral";
    }
    return "unknown";
}

std::string toString(EffectiveFidelity value) {
    switch (value) {
        case EffectiveFidelity::Unspecified: return "unspecified";
        case EffectiveFidelity::Structural: return "structural";
        case EffectiveFidelity::Behavioral: return "behavioral";
        case EffectiveFidelity::Mixed: return "mixed";
    }
    return "unspecified";
}

std::string toString(VerificationStatus value) {
    switch (value) {
        case VerificationStatus::Unverified: return "unverified";
        case VerificationStatus::Characterized: return "characterized";
        case VerificationStatus::Verified: return "verified";
        case VerificationStatus::ReferenceOnly: return "reference-only";
    }
    return "unverified";
}

std::string toString(UnavailableFidelityPolicy value) {
    switch (value) {
        case UnavailableFidelityPolicy::Error: return "error";
        case UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException:
            return "use-only-available-and-record-exception";
    }
    return "error";
}

std::string canonicalizeParameters(const ParameterMap& parameters) {
    std::ostringstream out;
    bool first = true;
    for (const auto& [key, value] : parameters) {
        if (!first) {
            out << ';';
        }
        first = false;
        out << key << '=' << value;
    }
    return out.str();
}

} // namespace circuit
