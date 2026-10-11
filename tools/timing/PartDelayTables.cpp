// Generates, or checks, the delay tables that compact parts run on.
//
// For every catalog contract that has a gate-level and a single-part compact
// implementation, the gate-level form is analysed with
// CircuitTimingAnalyzer::delays and the result is written as C++ for
// src/components/selection/ProvenPartDelays.generated.inc.
//
//   part_delay_tables --write FILE [--budget=N] [--clocked] [CONTRACT ...]
//   part_delay_tables --check      [--budget=N] [CONTRACT ...]
//   part_delay_tables --coverage
//   part_delay_tables --verify FILE   (recompute every entry; must equal FILE)
//
// --check recomputes the tables that are compiled in and fails on any
// difference. Storage and memory parts are not generated here: a flip-flop
// built from gates is a loop, and the timed memories carry their own timing.
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/BasicComponent.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/ProvenPartDelays.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include <fstream>
#include <iostream>
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
        gates = catalog.createRoot({candidate.contract, "DUT", parameters}, storage(candidate.contract)
            ? circuit::maximallyStructural()
            : rv32i::withBehavioralMemoryParts(circuit::maximallyStructural(), "part-delay-reference-v1")).root;
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
        && (candidate.contract == "rv32i.alu32" || candidate.contract == "rv32i.alu32-core" || candidate.contract == "alu.width8");
    auto table = CircuitTimingAnalyzer::delays(gates, options, per_operation ? "OP" : "", &diagnostics);
    if (table.empty()) reason = diagnostics.empty() ? "nothing changes" : diagnostics.front();
    return table;
}

// Parameter combinations that parts are really built with, each proven on
// its own: the advanced core for every branch-prediction policy.
std::vector<circuit::ParameterMap> variants(const std::string& contract) {
    std::vector<circuit::ParameterMap> result;
    if (contract == "rv32i.core.educational-five-stage")
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
// largest sizes that can be built from gates. Larger memories, and the
// scalable form of the same memory, are estimated from those two.
size_t sized(const Candidate& candidate, const circuit::PartDelays& standard, size_t budget, std::ostream& body) {
    const auto device = candidate.contract.find(".device."), endpoint = candidate.contract.find(".endpoint.");
    if (candidate.contract.rfind("memory.", 0) != 0 || (device == std::string::npos && endpoint == std::string::npos)) return 0;
    auto scalable = candidate.contract;
    if (device != std::string::npos) scalable.replace(device, 8, ".scalable-device.");
    else scalable.replace(endpoint, 10, ".scalable-endpoint.");
    const auto scalable_implementation = compactOf(scalable);
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
        for (const bool always_ready : {false, true}) {
            if (always_ready && !ready_variant) break;
            for (const size_t depth : {size_t{64}, size_t{128}}) {
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
    return written;
}
} // namespace

int main(int argc, char** argv) {
    try {
        std::string write, verify;
        bool check = false, clocked = false, coverage = false;
        size_t budget = 4000;
        std::set<std::string> only;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--write" && i + 1 < argc) write = argv[++i];
            else if (argument == "--verify" && i + 1 < argc) verify = argv[++i];
            else if (argument == "--check") check = true;
            else if (argument == "--clocked") clocked = true;
            else if (argument == "--coverage") coverage = true;
            else if (argument.rfind("--budget=", 0) == 0) budget = std::stoul(argument.substr(9));
            else only.insert(argument);
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
            return same ? 0 : 1;
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
