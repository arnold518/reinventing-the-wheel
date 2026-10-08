#include "components/selection/SelectionTypes.hpp"
#include <sstream>
#include <string_view>
#include <utility>

namespace circuit {

std::string toString(Fidelity value) {
    switch (value) {
        case Fidelity::Structural: return "structural";
        case Fidelity::Behavioral: return "behavioral";
    }
    return "unknown";
}

std::string toString(VerificationStatus value) {
    switch (value) {
        case VerificationStatus::Unverified: return "unverified";
        case VerificationStatus::Characterized: return "characterized";
        case VerificationStatus::Verified: return "verified";
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

std::string canonicalContractId(std::string_view id) {
    if (!id.starts_with("memory.")) return std::string(id);
    // October 2026: memory contracts follow memory.<technology>.<what>.<width>.
    static constexpr std::pair<std::string_view, std::string_view> prefixes[] = {
        {"memory.ff-word.", "memory.ff.word."},
        {"memory.ff-storage-bank.", "memory.ff.bank."},
        {"memory.container.ff.", "memory.ff.device."},
        {"memory.matrix.", "memory.shared."},
        {"memory.transaction-port.", "memory.shared.transaction-port."},
        {"memory.response-buffer.", "memory.shared.response-buffer."},
    };
    // Old order memory.<what>.<technology>.<width>.
    static constexpr std::pair<std::string_view, std::string_view> layers[] = {
        {"memory.device.", "scalable-device"},
        {"memory.controller.", "controller"},
        {"memory.endpoint.", "endpoint"},
        {"memory.scalable.", "scalable-endpoint"},
    };
    if (id == "memory.write-enabled-bit") return "memory.ff.cell";
    for (const auto& [old_prefix, new_prefix] : prefixes) {
        if (id.starts_with(old_prefix)) return std::string(new_prefix) + std::string(id.substr(old_prefix.size()));
    }
    for (const auto& [old_prefix, what] : layers) {
        if (!id.starts_with(old_prefix)) continue;
        const auto rest = id.substr(old_prefix.size());
        const auto dot = rest.rfind('.');
        if (dot == std::string_view::npos) break;
        return "memory." + std::string(rest.substr(0, dot)) + "." + std::string(what) + std::string(rest.substr(dot));
    }
    return std::string(id);
}

} // namespace circuit
