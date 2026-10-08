// Generates, or checks, the delay tables that compact parts run on.
//
// For every catalog contract that has a gate-level and a single-part compact
// implementation, the gate-level form is analysed with
// CircuitTimingAnalyzer::delays and the result is written as C++ for
// src/components/selection/ProvenPartDelays.generated.inc.
//
//   part_delay_tables --write FILE [--budget=N] [--clocked] [CONTRACT ...]
//   part_delay_tables --check      [--budget=N] [CONTRACT ...]
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
    for (const auto& [output, range] : t.launches)
        out << "    t.launches[" << quote(output) << "] = {" << range.minimum << ", " << range.maximum << "};\n";
    for (const auto& [input, setup] : t.setup) out << "    t.setup[" << quote(input) << "] = " << setup << ";\n";
    if (t.internal_period) out << "    t.internal_period = " << t.internal_period << ";\n";
    for (const auto& [input, value] : t.fixed_inputs) out << "    t.fixed_inputs[" << quote(input) << "] = " << value << ";\n";
    out << "    return t;\n}()},\n";
    return out.str();
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
        if (contract.id.rfind("memory.", 0) == 0 || contract.id.rfind("storage.", 0) == 0
            || contract.id.rfind("sequential.", 0) == 0 || contract.id.rfind("utility.", 0) == 0
            || contract.id.rfind("logic.", 0) == 0) continue;
        std::string compact;
        bool gates = false;
        for (const auto& implementation : catalog.implementationsFor(contract.id)) {
            if (implementation.fidelity == circuit::Fidelity::Behavioral) compact = implementation.id;
            else gates = true;
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
circuit::PartDelays prove(const Candidate& candidate, size_t budget, std::string& reason) {
    const auto& catalog = circuit::builtinComponentCatalog();
    std::shared_ptr<Component> gates;
    try {
        gates = catalog.createRoot({candidate.contract, "DUT", {}},
            rv32i::withBehavioralMemoryParts(circuit::maximallyStructural(), "part-delay-reference-v1")).root;
    } catch (const std::exception& error) { reason = error.what(); return {}; }
    const auto pins = std::dynamic_pointer_cast<IOComponent>(gates);
    if (!pins || pins->getAllOutputPins().empty()) { reason = "no outputs"; return {}; }
    SettlingOptions options;
    options.binary_sources = true;
    options.search_budget = budget;
    if (pins->getInputPinDynamic("RST")) options.fixed_inputs["RST"] = 0;
    std::vector<std::string> diagnostics;
    auto table = CircuitTimingAnalyzer::delays(gates, options, "", &diagnostics);
    if (table.empty()) reason = diagnostics.empty() ? "nothing changes" : diagnostics.front();
    return table;
}

} // namespace

int main(int argc, char** argv) {
    try {
        std::string write;
        bool check = false, clocked = false;
        size_t budget = 4000;
        std::set<std::string> only;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--write" && i + 1 < argc) write = argv[++i];
            else if (argument == "--check") check = true;
            else if (argument == "--clocked") clocked = true;
            else if (argument.rfind("--budget=", 0) == 0) budget = std::stoul(argument.substr(9));
            else only.insert(argument);
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
