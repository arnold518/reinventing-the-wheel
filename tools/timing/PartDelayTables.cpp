// Generates, or checks, the delay tables that compact parts run on.
//
// For every catalog contract that has a gate-level and a single-part compact
// implementation, the gate-level form is analysed with
// CircuitTimingAnalyzer::delays and the result is written as C++ for
// src/components/selection/ProvenPartDelays.generated.inc.
//
//   part_delay_tables [--profile=NAME] --write FILE [--budget=N] [--clocked] [CONTRACT ...]
//
//   --profile chooses the timing profile whose gate delays are analysed
//   (components/TimingProfile.hpp); the default is the active profile.
//   part_delay_tables --check      [--budget=N] [CONTRACT ...]
//   part_delay_tables --coverage
//   part_delay_tables --verify FILE   (recompute every entry; must equal FILE)
//   part_delay_tables --gate-lists FILE   (the parts that get no gate list,
//                                          for NoGateList.generated.inc)
//
// --check recomputes the tables that are compiled in and fails on any
// difference. Storage and memory parts are not generated here: a flip-flop
// built from gates is a loop, and the timed memories carry their own timing.
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "analysis/DelayTwin.hpp"
#include "components/BasicComponent.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/ProvenPartDelays.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace circuit::analysis;
namespace {

std::string quote(const std::string& text) { return "\"" + text + "\""; }

std::string emit(const std::string& implementation, const circuit::PartDelays& t) {
    std::ostringstream out;
    out << "{" << quote(implementation) << ", [] {\n    PartDelays t;\n    t.evidence = " << quote(t.evidence) << ";\n";
    if (!t.arcs.empty()) {
        out << "    t.arcs = {\n";
        for (const auto& arc : t.arcs)
            out << "        Arc{" << quote(arc.input) << ", " << quote(arc.output) << ", {" << arc.delay.minimum << ", "
                << arc.delay.maximum << "}},\n";
        out << "    };\n";
    }
    if (!t.clock.empty()) out << "    t.clock = " << quote(t.clock) << ";\n";
    for (const auto& output : t.clock_outputs) out << "    t.clock_outputs.push_back(" << quote(output) << ");\n";
    for (const auto& [output, range] : t.launches)
        out << "    t.launches[" << quote(output) << "] = {" << range.minimum << ", " << range.maximum << "};\n";
    for (const auto& [input, setup] : t.setup) out << "    t.setup[" << quote(input) << "] = " << setup << ";\n";
    if (t.self_timed) out << "    t.self_timed = true;\n";
    if (t.clocked_interface) out << "    t.clocked_interface = true;\n";
    if (t.internal_period) out << "    t.internal_period = " << t.internal_period << ";\n";
    for (const auto& [input, value] : t.fixed_inputs) out << "    t.fixed_inputs[" << quote(input) << "] = " << value << ";\n";
    if (!t.control.empty()) {
        out << "    t.control = " << quote(t.control) << ";\n";
        for (const auto& [value, arcs] : t.arcs_by_control) {
            out << "    t.arcs_by_control[" << value << "] = {";
            for (const auto& arc : arcs)
                out << "{" << quote(arc.input) << ", " << quote(arc.output) << ", {" << arc.delay.minimum << ", " << arc.delay.maximum << "}}, ";
            out << "};\n";
        }
    }
    out << "    return t;\n}()},\n";
    return out.str();
}

// The parts every structural view of a core keeps compact. They are built
// from the gate-level flip-flop, whose contract is measured exhaustively.
bool storage(const std::string& contract) {
    return contract.rfind("storage.", 0) == 0 || contract == "rv32i.register-file"
        || (contract.rfind("memory.ff.", 0) == 0 && contract.find(".scalable-") == std::string::npos)
        || (contract.rfind("memory.", 0) == 0 && contract.find(".scalable-") == std::string::npos);
}

struct Candidate {
    std::string contract, implementation;
};

// Contracts with a single-part compact form and a gate-level form.
std::vector<Candidate> candidates(const std::set<std::string>& only) {
    const auto& catalog = circuit::builtinComponentCatalog();
    std::vector<Candidate> result;
    for (const auto& contract : catalog.contracts()) {
        if (!only.empty() && !only.contains(contract.id)) continue;
        if ((contract.id.rfind("memory.", 0) == 0 && !storage(contract.id))
            || contract.id.rfind("sequential.", 0) == 0 || contract.id.rfind("utility.", 0) == 0) continue;
        // The compact cache controller proves its table when it is built, for
        // the parameters it is built with (src/modules/memory/Cache.cpp).
        if (contract.id.rfind("memory.cache.controller.", 0) == 0) continue;
        std::string compact;
        bool gates = false;
        for (const auto& implementation : catalog.implementationsFor(contract.id)) {
            if (implementation.fidelity == circuit::Fidelity::Behavioral) compact = implementation.id;
            else gates = !implementation.terminal_primitive; // a single gate is its own delay
        }
        if (compact.empty() || !gates) continue;
        try {
            const auto built = catalog.createRoot({contract.id, "DUT", {}}, circuit::strictAllBehavioral()).root;
            if (!std::dynamic_pointer_cast<BasicComponent>(built)) continue;
        } catch (const std::exception&) { continue; }
        result.push_back({contract.id, compact});
    }
    return result;
}

// The table of one part, proven on its gates. Storage and memory inside it
// stay compact, as in every structural view of a core.
circuit::PartDelays prove(const Candidate& candidate, size_t budget, std::string& reason,
                          const circuit::ParameterMap& parameters = {}) {
    const auto& catalog = circuit::builtinComponentCatalog();
    std::shared_ptr<Component> gates;
    try {
        auto profile = storage(candidate.contract)
            ? circuit::maximallyStructural()
            : rv32i::withBehavioralMemoryParts(circuit::maximallyStructural(), "part-delay-reference-v1");
        if (candidate.contract == "rv32im.core.educational-five-stage") {
            // Its compact evaluator observes real register-word nodes. Match
            // RV32IMCoreBehavioral's expanded register file with characterized
            // storage leaves, rather than the older I core's opaque RF.
            profile = circuit::withProfileOverrides(std::move(profile), {
                circuit::preferFidelity(circuit::Fidelity::Structural,
                    circuit::ProfileSelector::contract("rv32i.register-file"),
                    "retain RV32IM register-word observation nodes")}, "rv32im-part-delay-reference-v1");
        }
        gates = catalog.createRoot({candidate.contract, "DUT", parameters}, profile).root;
    } catch (const std::exception& error) { reason = error.what(); return {}; }
    // Storage keeps its compact form in this profile; a part that is itself
    // storage then has no gates to prove anything on.
    if (std::dynamic_pointer_cast<BasicComponent>(gates)) { reason = "storage: compact in the reference profile"; return {}; }
    const auto pins = std::dynamic_pointer_cast<IOComponent>(gates);
    if (!pins || pins->getAllOutputPins().empty()) { reason = "no outputs"; return {}; }
    // Proven for inputs that end at any of 0, 1, X or Z. Assuming 0 or 1
    // would remove a few false paths, but an X on a select line keeps both
    // sides live, and a compact part must not be faster than its gates then.
    SettlingOptions options;
    options.search_budget = budget;
    if (pins->getInputPinDynamic("RST")) options.fixed_inputs["RST"] = 0;
    std::vector<std::string> diagnostics;
    // A part whose work depends on an operation code also gets one set of
    // delays per code, so that a quick operation need not wait for a slow one.
    const auto operation = pins->getInputPinDynamic("OP");
    const bool per_operation = operation && operation->getWidth() <= 5
        && (candidate.contract == "rv32i.alu" || candidate.contract == "word.alu.8");
    // An SRAM device reads with WE_N=1 and writes with WE_N=0; proven apart,
    // a read is not charged for a path that runs through a write and back
    // through a cell.
    // (A clocked macro registers WE_N; its paths start at the clock.)
    const auto clocked = parameters.find("clocked");
    const bool per_write = candidate.contract.rfind("memory.sram.device.", 0) == 0
        && (clocked == parameters.end() || clocked->second != "1");
    auto table = CircuitTimingAnalyzer::delays(gates, options, per_operation ? "OP" : per_write ? "WE_N" : "", &diagnostics);
    if (table.empty()) reason = diagnostics.empty() ? "nothing changes" : diagnostics.front();
    return table;
}

// Parameter combinations that parts are really built with, each proven on
// its own: the advanced core for every branch-prediction policy.
std::vector<circuit::ParameterMap> variants(const std::string& contract) {
    std::vector<circuit::ParameterMap> result;
    if (contract == "rv32i.core.educational-five-stage" || contract == "rv32im.core.educational-five-stage")
        for (const char* policy : {"not-taken", "taken", "backward-taken", "one-bit", "two-bit"})
            for (const char* entries : {"1", "2", "4", "8", "16", "32", "64", "128", "256"})
                result.push_back({{"prediction", policy}, {"prediction_entries", entries}});
    return result;
}
std::string variantKey(const std::string& implementation, const circuit::ParameterMap& parameters) {
    auto key = implementation;
    for (const auto& [name, value] : parameters) key += (key == implementation ? "?" : "&") + name + "=" + value;
    return key;
}

// The compact implementation of a contract, if it has one.
std::string compactOf(const std::string& contract) {
    const auto& catalog = circuit::builtinComponentCatalog();
    if (!catalog.hasContract(contract)) return {};
    for (const auto& implementation : catalog.implementationsFor(contract))
        if (implementation.fidelity == circuit::Fidelity::Behavioral) return implementation.id;
    return {};
}

// Memory devices and endpoints are also proven at 64 and 128 words, the two
// largest sizes that can be built from gates. Larger memories use those
// measured device tables; scalable endpoints reuse the endpoint tables.
// The clocked on-chip SRAM macro ("@clocked"): its controller, and its
// device and endpoints at the default size and at 64 to 512 words, in the
// default row layout.
size_t clockedSRAM(const Candidate& candidate, const std::string& scalable_implementation, size_t budget, std::ostream& body) {
    if (candidate.contract.rfind("memory.sram.", 0) != 0) return 0;
    const bool controller = candidate.contract.find(".controller.") != std::string::npos;
    size_t written = 0;
    const auto write = [&](const std::string& suffix, const circuit::PartDelays& table) {
        body << emit(candidate.implementation + suffix, table); ++written;
        if (!scalable_implementation.empty()) { body << emit(scalable_implementation + suffix, table); ++written; }
    };
    std::string reason;
    const auto table = prove(candidate, budget, reason, {{"clocked", "1"}});
    if (table.empty()) std::cerr << "  no clocked table for " << candidate.contract << ": " << reason << '\n';
    else write("@clocked", table);
    if (controller) return written;
    for (const size_t depth : {size_t{64}, size_t{128}, size_t{256}, size_t{512}}) {
        std::string sized_reason;
        const auto at = prove(candidate, budget, sized_reason, {{"depth_words", std::to_string(depth)}, {"clocked", "1"}});
        if (at.empty()) { std::cerr << "  no clocked " << depth << "-word table for " << candidate.contract << ": " << sized_reason << '\n'; continue; }
        write("@depth=" + std::to_string(depth) + "@clocked", at);
    }
    return written;
}

size_t sized(const Candidate& candidate, const circuit::PartDelays& standard, size_t budget, std::ostream& body) {
    const auto device = candidate.contract.find(".device."), endpoint = candidate.contract.find(".endpoint.");
    if (candidate.contract.rfind("memory.sram.controller.", 0) == 0) return clockedSRAM(candidate, {}, budget, body);
    if (candidate.contract.rfind("memory.", 0) != 0 || (device == std::string::npos && endpoint == std::string::npos)) return 0;
    auto scalable = candidate.contract;
    if (endpoint != std::string::npos) scalable.replace(endpoint, 10, ".scalable-endpoint.");
    const auto scalable_implementation = endpoint != std::string::npos ? compactOf(scalable) : std::string{};
    size_t written = 0;
    // The scalable form with default parameters is the same small memory.
    if (!scalable_implementation.empty()) { body << emit(scalable_implementation, standard); ++written; }
    // A flip-flop endpoint whose consumer is always ready is a different
    // circuit, used by the fixed-cycle cores; it is proven as its own variant.
    // Every other technology is also proven for each row layout.
    const bool flip_flop = candidate.contract.rfind("memory.ff.", 0) == 0;
    const bool ready_variant = candidate.contract.rfind("memory.ff.endpoint.", 0) == 0;
    for (const char* layout : {"2", "1", "4", "8", "16"}) {
        const bool standard_layout = std::string(layout) == "2";
        if (!standard_layout && flip_flop) break;
        const std::string rows = standard_layout ? "" : std::string("@rows=") + layout;
        if (!standard_layout) {
            // The default size in this layout (where the layout fits it).
            std::string reason;
            const auto table = prove(candidate, budget, reason, {{"words_per_row", layout}});
            if (!table.empty()) {
                body << emit(candidate.implementation + rows, table);
                ++written;
                if (!scalable_implementation.empty()) { body << emit(scalable_implementation + rows, table); ++written; }
            }
        }
        // Four-word bursts (a cache line of four words), in the default
        // layout and the long-row one; a memory whose bursts may not be that
        // long in this layout (DRAM bursts stay in one row) has none.
        if (!flip_flop && (standard_layout || std::string(layout) == "16")) {
            const std::string block = rows + "@block=4";
            for (const size_t depth : {size_t{0}, size_t{64}, size_t{128}}) {
                circuit::ParameterMap parameters{{"block_words", "4"}};
                if (!standard_layout) parameters["words_per_row"] = layout;
                if (depth) parameters["depth_words"] = std::to_string(depth);
                std::string reason;
                const auto table = prove(candidate, budget, reason, parameters);
                if (table.empty()) { std::cerr << "  no four-word-burst " << depth << "-word table for " << candidate.contract << rows << ": " << reason << '\n'; continue; }
                const auto suffix = (depth ? "@depth=" + std::to_string(depth) : std::string{}) + block;
                body << emit(candidate.implementation + suffix, table);
                ++written;
                if (!scalable_implementation.empty()) { body << emit(scalable_implementation + suffix, table); ++written; }
            }
        }
        for (const bool always_ready : {false, true}) {
            if (always_ready && !ready_variant) break;
            // On-chip SRAM is also proven at 256 and 512 words (4 and 8
            // subarrays), where it fits the gate-level limit: the subarray
            // formula is fitted at 1, 2 and 4 and checked at 8.
            const bool subarrays = standard_layout && !always_ready && candidate.contract.rfind("memory.sram.", 0) == 0;
            for (const size_t depth : {size_t{64}, size_t{128}, size_t{256}, size_t{512}}) {
                if (depth > 128 && !subarrays) break;
                circuit::ParameterMap parameters{{"depth_words", std::to_string(depth)}};
                if (!standard_layout) parameters["words_per_row"] = layout;
                if (always_ready) parameters["always_ready"] = "1";
                std::string reason;
                const auto table = prove(candidate, budget, reason, parameters);
                if (table.empty()) { std::cerr << "  no " << depth << "-word table for " << candidate.contract << rows << ": " << reason << '\n'; continue; }
                const auto suffix = "@depth=" + std::to_string(depth) + rows + (always_ready ? "@always_ready" : "");
                body << emit(candidate.implementation + suffix, table);
                ++written;
                if (!scalable_implementation.empty()) { body << emit(scalable_implementation + suffix, table); ++written; }
            }
        }
    }
    return written + clockedSRAM(candidate, scalable_implementation, budget, body);
}

// The parts whose gate-level form gives no gate list (too large, a loop, a
// stored state that is not a flip-flop). Finding that out means building all
// their gates, so the catalog reads the answer from a generated file.
std::set<std::string> withoutGateList(std::map<std::string, std::string>* reasons = nullptr) {
    // Parts that could have a gate list but are better off without: measured,
    // the list changes nothing and costs run time.
    static const std::map<std::string, std::string> NO_GAIN = {
        {"rv32i.register-file", "no gain: the same shares on its table (DelayRuleTest), and its large gate list made gate-level core runs 21% slower"},
    };
    std::set<std::string> large, seen;
    for (const auto& [contract, why] : NO_GAIN) {
        large.insert(contract); seen.insert(contract);
        if (reasons) (*reasons)[contract] = why;
    }
    for (const auto& candidate : candidates({})) {
        if (!seen.insert(candidate.contract).second) continue;
        try {
            const auto gates = circuit::builtinComponentCatalog().createRoot({candidate.contract, "DUT", {}}, circuit::maximallyStructural()).root;
            std::string why;
            if (!gates || std::dynamic_pointer_cast<BasicComponent>(gates) || circuit::analysis::buildDelayTwin(gates, &why)) continue;
            large.insert(candidate.contract);
            if (reasons) (*reasons)[candidate.contract] = why;
        } catch (const std::exception&) {}
    }
    return large;
}
} // namespace

int main(int argc, char** argv) {
    try {
        std::string write, verify, gate_lists;
        bool check = false, clocked = false, coverage = false;
        size_t budget = 4000;
        std::set<std::string> only;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--write" && i + 1 < argc) write = argv[++i];
            else if (argument == "--verify" && i + 1 < argc) verify = argv[++i];
            else if (argument == "--gate-lists" && i + 1 < argc) gate_lists = argv[++i];
            else if (argument == "--check") check = true;
            else if (argument == "--clocked") clocked = true;
            else if (argument == "--coverage") coverage = true;
            else if (argument.rfind("--budget=", 0) == 0) budget = std::stoul(argument.substr(9));
            else if (argument.rfind("--profile=", 0) == 0) circuit::timing::setActiveTimingProfile(argument.substr(10));
            else only.insert(argument);
        }
        if (!gate_lists.empty()) {
            std::ofstream out(gate_lists);
            out << "// Generated by part_delay_tables --gate-lists. Do not edit by hand.\n";
            std::map<std::string, std::string> reasons;
            const auto large = withoutGateList(&reasons);
            for (const auto& contract : large) out << quote(contract) << ", // " << reasons[contract] << "\n";
            std::cout << large.size() << " parts without a gate list written to " << gate_lists << '\n';
            return 0;
        }
        if (coverage) {
            // Every compact implementation in the catalog, and whether it
            // runs on a proven table.
            const auto& catalog = circuit::builtinComponentCatalog();
            size_t with = 0, without = 0;
            for (const auto& contract : catalog.contracts()) {
                bool gates = false;
                for (const auto& implementation : catalog.implementationsFor(contract.id))
                    gates = gates || (implementation.fidelity != circuit::Fidelity::Behavioral && !implementation.terminal_primitive);
                for (const auto& implementation : catalog.implementationsFor(contract.id)) {
                    if (implementation.fidelity != circuit::Fidelity::Behavioral) continue;
                    if (circuit::provenPartDelays(implementation.id, {})) { ++with; continue; }
                    std::string reason;
                    std::shared_ptr<Component> built;
                    try { built = catalog.createRoot({contract.id, "DUT", {}}, circuit::strictAllBehavioral()).root; }
                    catch (const std::exception&) { reason = "needs parameters"; }
                    if (reason.empty() && !std::dynamic_pointer_cast<BasicComponent>(built)) reason = "compact form is built from several parts";
                    if (reason.empty() && !gates) reason = "no gate-level form to match";
                    if (reason.empty()) { prove({contract.id, implementation.id}, budget, reason); if (reason.empty()) reason = "not generated yet"; }
                    ++without;
                    std::cout << "no table: " << implementation.id << ": " << reason << '\n';
                }
            }
            std::cout << with << " compact implementations run on a proven table; " << without << " do not\n";
            return 0;
        }
        if (!verify.empty()) {
            // Every stored table, recomputed from the gates and compared as text.
            std::ostringstream fresh;
            fresh << "// Generated by part_delay_tables. Do not edit by hand.\n";
            size_t entries = 0;
            for (const auto& candidate : candidates({})) {
                std::string reason;
                const auto table = prove(candidate, budget, reason);
                if (table.empty()) continue;
                fresh << emit(candidate.implementation, table);
                entries += 1 + sized(candidate, table, budget, fresh);
                for (const auto& parameters : variants(candidate.contract)) {
                    std::string variant_reason;
                    const auto variant = prove(candidate, budget, variant_reason, parameters);
                    if (variant.empty()) continue;
                    fresh << emit(variantKey(candidate.implementation, parameters), variant);
                    ++entries;
                }
            }
            std::ifstream stored(verify);
            std::stringstream installed;
            installed << stored.rdbuf();
            const bool same = installed.str() == fresh.str();
            std::cout << entries << " table entries recomputed from the gates: "
                      << (same ? "identical to the stored file" : "DIFFERENT from the stored file") << '\n';
            const bool same_list = withoutGateList() == circuit::partsWithoutGateList();
            std::cout << "parts without a gate list: " << (same_list ? "as stored" : "DIFFERENT from NoGateList.generated.inc") << '\n';
            return same && same_list ? 0 : 1;
        }
        if (write.empty() == !check) throw std::invalid_argument("Choose --write FILE or --check");

        if (check) {
            size_t checked = 0, different = 0;
            const auto& catalog = circuit::builtinComponentCatalog();
            for (const auto& contract : catalog.contracts()) {
                if (!only.empty() && !only.contains(contract.id)) continue;
                for (const auto& implementation : catalog.implementationsFor(contract.id)) {
                    const auto* stored = circuit::provenPartDelays(implementation.id, {});
                    if (!stored) continue;
                    std::string reason;
                    const auto proven = prove({contract.id, implementation.id}, budget, reason);
                    ++checked;
                    if (emit(implementation.id, proven) == emit(implementation.id, *stored)) continue;
                    ++different;
                    std::cerr << implementation.id << ": stored table differs from the gates"
                              << (reason.empty() ? "" : " (" + reason + ")") << '\n';
                }
            }
            std::cout << checked << " stored tables recomputed from the gates; " << different << " differ\n";
            return different ? 1 : 0;
        }

        std::ostringstream body;
        body << "// Generated by part_delay_tables. Do not edit by hand.\n";
        size_t written = 0;
        for (const auto& candidate : candidates(only)) {
            std::string reason;
            const auto table = prove(candidate, budget, reason);
            if (table.empty()) { std::cerr << "skipped " << candidate.contract << ": " << reason << '\n'; continue; }
            if (!table.clock.empty() && !clocked) { std::cerr << "skipped " << candidate.contract << ": clocked\n"; continue; }
            body << emit(candidate.implementation, table);
            ++written;
            written += sized(candidate, table, budget, body);
            for (const auto& parameters : variants(candidate.contract)) {
                std::string variant_reason;
                const auto variant = prove(candidate, budget, variant_reason, parameters);
                const auto key = variantKey(candidate.implementation, parameters);
                if (variant.empty()) { std::cerr << "  no table for " << key << ": " << variant_reason << '\n'; continue; }
                body << emit(key, variant);
                ++written;
                std::cerr << "  proved " << key << '\n';
            }
            std::cerr << "proved " << candidate.contract << " (" << table.arcs.size() << " pairs"
                      << (table.clock.empty() ? "" : ", clocked") << ")\n";
        }
        std::ofstream(write) << body.str();
        std::cout << written << " tables written to " << write << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "part_delay_tables failed: " << error.what() << '\n';
        return 1;
    }
}
