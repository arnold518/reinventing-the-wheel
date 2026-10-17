#include "components/selection/ProvenPartDelays.hpp"
#include "components/TimingProfile.hpp"
#include <algorithm>
#include <map>
#include <set>

namespace circuit {
namespace {

using Arc = PartDelays::Arc;

// One generated file per timing profile, each compiled in its own source
// file (ProvenPartDelaysUnit.cpp and its siblings): together in one file the
// tables need too much compiler memory for this host. A table is proven on
// the gates as they are timed in that profile. A profile without a file has
// no tables; its compact parts then keep their single delay.
const std::map<std::string, PartDelays>& tables() {
    static const std::map<std::string, PartDelays> none;
    const auto& profile = timing::activeTimingProfile().name;
    if (profile == "unit") return provenPartDelaysUnit();
    if (profile == "embedded") return provenPartDelaysEmbedded();
    if (profile == "desktop") return provenPartDelaysDesktop();
    return none;
}

// Memory parameters that leave the timed circuit as it was proven: host
// budgets and device phase timings do not touch the gate paths, and a ROM
// image only sets constants behind the read column (the tables are the same
// for every image). The row layout and the burst block are proven variants
// (below).
bool sameCircuit(const ParameterMap& parameters) {
    static const std::set<std::string> untimed = {
        "depth_words", "paged", "record_history", "max_resident_bytes", "max_history_bytes",
        "phase_ticks", "guard_ticks", "program_ticks", "erase_ticks", "retention_ticks",
        "refresh_interval_ticks", "refresh_interval_cycles", "access_cycles", "always_ready", "words_per_row", "block_words", "image_hex",
        // A clocked SRAM macro is its own proven variant (the "@clocked" key).
        "clocked"};
    for (const auto& [name, value] : parameters)
        if (!untimed.contains(name)) return false;
    return true;
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

// A wider array's table from its own two-subarray table plus the growth of
// the 8-bit array from two subarrays to `grown`: the subarray decoder, the
// global wires and the return bus do not depend on the word width (the bits
// travel side by side), and a 16- or 32-bit array of four subarrays exceeds
// the gate-level limit.
PartDelays widerArray(const PartDelays& two, const PartDelays& narrow_two, const PartDelays& grown, size_t subarrays) {
    PartDelays table = two;
    const auto growth = [](size_t from, size_t to) { return to > from ? to - from : size_t{0}; };
    const auto find_arc = [](const PartDelays& t, const PartDelays::Arc& arc) -> const PartDelays::Arc* {
        for (const auto& candidate : t.arcs) if (candidate.input == arc.input && candidate.output == arc.output) return &candidate;
        return nullptr;
    };
    for (auto& arc : table.arcs) {
        const auto *from = find_arc(narrow_two, arc), *to = find_arc(grown, arc);
        if (from && to) arc.delay.maximum += growth(from->delay.maximum, to->delay.maximum);
    }
    for (auto& [output, range] : table.launches) {
        const auto from = narrow_two.launches.find(output), to = grown.launches.find(output);
        if (from != narrow_two.launches.end() && to != grown.launches.end()) range.maximum += growth(from->second.maximum, to->second.maximum);
    }
    for (auto& [input, setup] : table.setup) {
        const auto from = narrow_two.setup.find(input), to = grown.setup.find(input);
        if (from != narrow_two.setup.end() && to != grown.setup.end()) setup += growth(from->second, to->second);
    }
    table.internal_period += growth(narrow_two.internal_period, grown.internal_period);
    table.evidence = "ESTIMATE for " + std::to_string(subarrays) + " subarrays: proven at two subarrays, the 8-bit array's growth from two ("
        + two.evidence + ")";
    return table;
}

size_t gridSide(size_t subarrays) { size_t side = 1; while (side * side < subarrays) ++side; return side; }
size_t log2Ceil(size_t n) { size_t bits = 0; while ((size_t{1} << bits) < n) ++bits; return bits; }

} // namespace

PartDelays sramSubarrayEstimate(const PartDelays& one, const PartDelays& two, const PartDelays& four, size_t subarrays,
                                size_t wire_per_side) {
    PartDelays table = four;
    const long long levels = static_cast<long long>(log2Ceil(subarrays)) - 2;
    const long long sides = static_cast<long long>(gridSide(subarrays)) - static_cast<long long>(gridSide(4));
    const auto grow = [&](size_t d4, size_t d2, size_t d1) {
        if (d4 == d2 && d2 == d1) return d4;
        const long long a = std::max<long long>(0, static_cast<long long>(d4) - static_cast<long long>(d2));
        const long long wire = 2 * static_cast<long long>(wire_per_side);
        return static_cast<size_t>(std::max<long long>(static_cast<long long>(d4), static_cast<long long>(d4) + a * levels + wire * sides));
    };
    const auto find_arc = [](const PartDelays& t, const PartDelays::Arc& arc) -> const PartDelays::Arc* {
        for (const auto& candidate : t.arcs) if (candidate.input == arc.input && candidate.output == arc.output) return &candidate;
        return nullptr;
    };
    for (auto& arc : table.arcs) {
        const auto* at2 = find_arc(two, arc);
        const auto* at1 = find_arc(one, arc);
        arc.delay.maximum = grow(arc.delay.maximum, at2 ? at2->delay.maximum : arc.delay.maximum, at1 ? at1->delay.maximum : (at2 ? at2->delay.maximum : arc.delay.maximum));
        if (at1) arc.delay.minimum = std::min(arc.delay.minimum, at1->delay.minimum);
    }
    for (auto& [output, range] : table.launches) {
        const auto at2 = two.launches.find(output), at1 = one.launches.find(output);
        const size_t d2 = at2 != two.launches.end() ? at2->second.maximum : range.maximum;
        range.maximum = grow(range.maximum, d2, at1 != one.launches.end() ? at1->second.maximum : d2);
        if (at1 != one.launches.end()) range.minimum = std::min(range.minimum, at1->second.minimum);
    }
    for (auto& [input, setup] : table.setup) {
        const auto at2 = two.setup.find(input), at1 = one.setup.find(input);
        const size_t d2 = at2 != two.setup.end() ? at2->second : setup;
        setup = grow(setup, d2, at1 != one.setup.end() ? at1->second : d2);
    }
    table.internal_period = grow(four.internal_period, two.internal_period, one.internal_period);
    table.evidence = "ESTIMATE for " + std::to_string(subarrays) + " subarrays: per-level growth fitted at 2 and 4 subarrays, global wire from the profile ("
        + four.evidence + ")";
    return table;
}

const PartDelays* provenPartDelays(const std::string& implementation_id, const ParameterMap& parameters) {
    const auto exact = [&](const std::string& key) -> const PartDelays* {
        const auto found = tables().find(key);
        return found == tables().end() ? nullptr : &found->second;
    };
    if (parameters.empty()) return exact(implementation_id);
    // A part proven for these exact parameters (for example the advanced
    // core for one branch-prediction policy).
    std::string variant_key = implementation_id;
    for (const auto& [name, value] : parameters) variant_key += (variant_key == implementation_id ? "?" : "&") + name + "=" + value;
    if (const auto* proven = exact(variant_key)) return proven;
    if (implementation_id.rfind("memory.", 0) != 0 || !sameCircuit(parameters)) return nullptr;
    // The row layout is part of the circuit, except for flip-flop memory,
    // which has none. Each layout other than the default is proven on its own.
    std::string rows;
    if (const auto layout = parameters.find("words_per_row");
        layout != parameters.end() && layout->second != "2" && implementation_id.find(".ff.") == std::string::npos) {
        rows = "@rows=" + layout->second;
        // Rows longer than gates can build (datasheet DRAM rows): the
        // 16-word layout's table. The longer row only changes the address
        // fields' wiring and shortens the open-row comparator.
        try { if (std::stoull(layout->second) > 16) rows = "@rows=16"; } catch (const std::exception&) {}
    }
    // A burst block other than the default two words changes the burst
    // counters: proven on its own (four words, in the default layout and in
    // the 16-word one that long rows use).
    if (const auto block = parameters.find("block_words"); block != parameters.end() && block->second != "2")
        rows += "@block=" + block->second;
    const auto clocked_parameter = parameters.find("clocked");
    const std::string clocked = clocked_parameter != parameters.end() && clocked_parameter->second == "1" ? "@clocked" : "";
    // No word count: the default size, as proven.
    if (!parameters.contains("depth_words")) {
        const auto ready_default = parameters.find("always_ready");
        return ready_default != parameters.end() && ready_default->second == "1" ? nullptr : exact(implementation_id + rows + clocked);
    }
    // A flip-flop endpoint whose consumer is always ready is a different
    // circuit (no response is kept), proven separately.
    const auto ready = parameters.find("always_ready");
    std::string variant = ready != parameters.end() && ready->second == "1" ? "@always_ready" : "";
    // Only the endpoint is built differently for an always-ready consumer;
    // a device inside it is the same circuit either way.
    if (!variant.empty() && !exact(implementation_id + "@depth=64" + variant)) variant.clear();
    variant = rows + variant + clocked;
    const auto* at64 = exact(implementation_id + "@depth=64" + variant);
    const auto* at128 = exact(implementation_id + "@depth=128" + variant);
    if (!at64 || !at128) return nullptr;
    size_t depth = 0;
    try { depth = std::stoull(parameters.at("depth_words")); } catch (const std::exception&) { return nullptr; }
    // 64 words or fewer: the table proven at 64 words, not the looser one
    // for 128.
    if (depth <= 64) return at64;
    // A size proven exactly (on-chip SRAM is proven up to 512 words).
    if (const auto* proven = exact(implementation_id + "@depth=" + std::to_string(depth) + variant)) return proven;
    static std::map<std::string, PartDelays> estimated;
    const auto key = timing::activeTimingProfile().name + "|" + implementation_id + "@depth=" + std::to_string(depth) + variant;
    auto found = estimated.find(key);
    if (found == estimated.end()) {
        // On-chip SRAM in the default layout (two words per row): 64, 128 and
        // 256 words are 1, 2 and 4 subarrays of SRAM_SUBARRAY_ROWS rows.
        const auto* at256 = exact(implementation_id + "@depth=256" + variant);
        const bool sram = implementation_id.rfind("memory.sram.", 0) == 0 && rows.empty();
        const size_t subarrays = (depth / 2 + 31) / 32;
        const size_t wire = timing::activeTimingProfile().memory.sram_wire_per_side;
        // The same part 8 bits wide, for a wider array's growth.
        std::string narrow = implementation_id;
        for (const char* width : {".16.", ".32."})
            if (const auto at = narrow.find(width); at != std::string::npos) narrow.replace(at, std::string(width).size(), ".8.");
        const auto* narrow64 = exact(narrow + "@depth=64" + variant);
        const auto* narrow128 = exact(narrow + "@depth=128" + variant);
        const auto* narrow256 = exact(narrow + "@depth=256" + variant);
        if (sram && at256) found = estimated.emplace(key, sramSubarrayEstimate(*at64, *at128, *at256, subarrays, wire)).first;
        else if (sram && narrow != implementation_id && narrow64 && narrow128 && narrow256)
            found = estimated.emplace(key, widerArray(*at128, *narrow128,
                sramSubarrayEstimate(*narrow64, *narrow128, *narrow256, std::max<size_t>(subarrays, 4), wire), subarrays)).first;
        else found = estimated.emplace(key, scaled(*at64, *at128, depth)).first;
    }
    return &found->second;
}

const std::set<std::string>& partsWithoutGateList() {
    static const std::set<std::string> generated = {
#include "NoGateList.generated.inc"
    };
    return generated;
}

std::vector<std::string> provenPartDelayImplementations() {
    std::vector<std::string> names;
    for (const auto& [name, table] : tables()) names.push_back(name);
    return names;
}

} // namespace circuit
