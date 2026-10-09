#include "components/selection/ProvenPartDelays.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace circuit {
namespace {

using Arc = PartDelays::Arc;

const std::map<std::string, PartDelays>& tables() {
    static const std::map<std::string, PartDelays> generated = {
#include "ProvenPartDelays.generated.inc"
    };
    return generated;
}

// A memory built with a word count. True when its other parameters leave the
// timed circuit as it was proven: host budgets and device phase timings do
// not touch the gate paths; the row geometry must be the default one; and a
// ROM image must be empty, because a table proven on one image says nothing
// about another.
bool sizedMemory(const std::string& implementation_id, const ParameterMap& parameters) {
    static const std::set<std::string> untimed = {
        "depth_words", "paged", "record_history", "max_resident_bytes", "max_history_bytes",
        "phase_ticks", "guard_ticks", "program_ticks", "erase_ticks", "retention_ticks",
        "refresh_interval_ticks", "always_ready", "words_per_row", "block_words", "image_hex"};
    for (const auto& [name, value] : parameters) {
        if (!untimed.contains(name)) return false;
        if ((name == "words_per_row" || name == "block_words") && value != "2") return false;
        if (name == "image_hex" && !value.empty() && implementation_id.find(".rom.") != std::string::npos) return false;
    }
    return parameters.contains("depth_words");
}

// A memory of `depth` words, from the tables proven at 64 and 128 words: each
// delay keeps growing per doubling as it did between those two. A memory this
// large cannot be built from gates, so this is an estimate, not a proof.
PartDelays scaled(const PartDelays& at64, const PartDelays& at128, size_t depth) {
    size_t doublings = 0;
    for (size_t words = 128; words < depth; words *= 2) ++doublings;
    PartDelays table = at128;
    const auto grow = [&](size_t large, size_t small) {
        return large + (large > small ? large - small : 0) * doublings;
    };
    for (auto& arc : table.arcs)
        for (const auto& before : at64.arcs) if (before.input == arc.input && before.output == arc.output) {
            arc.delay.maximum = grow(arc.delay.maximum, before.delay.maximum);
            arc.delay.minimum = std::min(arc.delay.minimum, before.delay.minimum);
        }
    for (auto& [output, range] : table.launches)
        if (const auto before = at64.launches.find(output); before != at64.launches.end()) {
            range.maximum = grow(range.maximum, before->second.maximum);
            range.minimum = std::min(range.minimum, before->second.minimum);
        }
    for (auto& [input, setup] : table.setup)
        if (const auto before = at64.setup.find(input); before != at64.setup.end()) setup = grow(setup, before->second);
    table.internal_period = grow(table.internal_period, at64.internal_period);
    if (doublings)
        table.evidence = "ESTIMATE for " + std::to_string(depth) + " words: proven at 64 and 128 words, growth per doubling continued ("
            + at128.evidence + ")";
    return table;
}

} // namespace

const PartDelays* provenPartDelays(const std::string& implementation_id, const ParameterMap& parameters) {
    const auto exact = [&](const std::string& key) -> const PartDelays* {
        const auto found = tables().find(key);
        return found == tables().end() ? nullptr : &found->second;
    };
    if (parameters.empty()) return exact(implementation_id);
    if (!sizedMemory(implementation_id, parameters)) return nullptr;
    // A flip-flop endpoint whose consumer is always ready is a different
    // circuit (no response is kept), proven separately.
    const auto ready = parameters.find("always_ready");
    const std::string variant = ready != parameters.end() && ready->second == "1" ? "@always_ready" : "";
    const auto* at64 = exact(implementation_id + "@depth=64" + variant);
    const auto* at128 = exact(implementation_id + "@depth=128" + variant);
    if (!at64 || !at128) return nullptr;
    size_t depth = 0;
    try { depth = std::stoull(parameters.at("depth_words")); } catch (const std::exception&) { return nullptr; }
    static std::map<std::string, PartDelays> estimated;
    const auto key = implementation_id + "@depth=" + std::to_string(depth) + variant;
    auto found = estimated.find(key);
    if (found == estimated.end()) found = estimated.emplace(key, scaled(*at64, *at128, depth)).first;
    return &found->second;
}

std::vector<std::string> provenPartDelayImplementations() {
    std::vector<std::string> names;
    for (const auto& [name, table] : tables()) names.push_back(name);
    return names;
}

} // namespace circuit
