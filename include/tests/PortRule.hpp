#pragma once

#include "components/selection/SelectionTypes.hpp"
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace circuit::test {

// The port rule every scenario checks first: reset holds every handshake
// output low; once reset is released, no responder has an answer pending.
// Readiness is not checked here: a responder may wait for a request before
// raising REQ_READY (the memory arbiter does), which the handshake allows.
inline std::pair<std::map<std::string, uint64_t>, std::map<std::string, uint64_t>>
portRuleExpectations(const std::vector<PinDescriptor>& pins) {
    std::map<std::string, uint64_t> reset, ready;
    std::set<std::string> outputs;
    for (const auto& p : pins) if (p.direction == PinType::OUTPUT) outputs.insert(p.name);
    const auto has = [&](const std::string& n) { return outputs.contains(n); };
    for (const auto& p : pins) {
        if (!p.name.ends_with("REQ_VALID")) continue;
        const auto prefix = p.name.substr(0, p.name.size() - 9);
        if (p.direction == PinType::INPUT) {           // a responder port
            for (const auto* n : {"REQ_READY", "RSP_VALID"}) if (has(prefix + n)) reset[prefix + n] = 0;
            if (has(prefix + "RSP_VALID")) ready[prefix + "RSP_VALID"] = 0;
        } else {                                       // a requester port
            for (const auto* n : {"REQ_VALID", "RSP_READY"}) if (has(prefix + n)) reset[prefix + n] = 0;
        }
    }
    return {reset, ready};
}

} // namespace circuit::test
