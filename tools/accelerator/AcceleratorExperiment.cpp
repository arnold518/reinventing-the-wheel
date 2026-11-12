// Chapter 6 measurements.
//
//   accelerator_experiment --parts
//       Gate counts and delays of the arithmetic parts (§ 6.1–6.2) under the
//       timing profile CIRCUITSIM_TIMING_PROFILE (unit by default).
//   accelerator_experiment --program NAME [--matrix 0|4|8|16] [--memory ff|sram|dram]
//                          [--period TICKS] [--array-multiplier] [--pipelined-bus]
//       Runs tests/fixtures/accelerator/NAME.elf on the accelerator machine
//       (compact) and prints the run as JSON: cycles, instructions, the
//       program's timed sections, its status, host seconds and events.
#include "runtime/AcceleratorGuest.hpp"
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/BasicComponent.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/accelerator/MatrixParts.hpp"
#include "modules/accelerator/MatrixUnit.hpp"
#include "modules/composite/ArrayMultiplier.hpp"
#include "modules/rv32im/MulDiv.hpp"
#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace {
using namespace circuit;

// Primitive gates and storage leaves in the all-gates build (adapters excluded).
size_t gates(const std::shared_ptr<Component>& c) {
    const std::string type = c->getTypeName();
    if (type == "Rewire" || type == "ConstantValue") return 0;
    if (c->getChildren().empty()) return 1;
    size_t n = 0;
    for (const auto& child : c->getChildren()) n += gates(child);
    return n;
}
std::shared_ptr<Component> allGates(const ComponentFamily& f, ParameterMap p = {}) {
    return builtinComponentCatalog().createRoot(f.request("PART", std::move(p)), strictAllStructural()).root;
}
SimulationTime latest(const PartDelays& d) {
    SimulationTime worst = 0;
    for (const auto& arc : d.arcs) worst = std::max(worst, arc.delay.maximum);
    for (const auto& [_, range] : d.launches) worst = std::max(worst, range.maximum);
    return worst;
}
void part(std::ostream& out, const std::string& name, const ComponentFamily& f, bool timing, const std::string& note, bool& first) {
    const auto root = allGates(f);
    out << (first ? "" : ",\n") << "  {\"part\": \"" << name << "\", \"gates\": " << gates(root);
    if (timing) {
        analysis::SettlingOptions options;
        const auto d = analysis::CircuitTimingAnalyzer::delays(root, options);
        out << ", \"latest_delay\": " << latest(d) << ", \"internal_period\": " << d.internal_period;
    }
    out << ", \"note\": \"" << note << "\"}" << std::flush;
    std::cerr << name << " measured\n";
    first = false;
}

int parts() {
    std::ostream& out = std::cout;
    out << "{\"profile\": \"" << timing::activeTimingProfile().name << "\", \"parts\": [\n";
    bool first = true;
    for (size_t w : {8u, 16u, 32u}) {
        part(out, "Multiplier<" + std::to_string(w) + ">", families::multiplier(w), false,
             "iterative: one bit per edge, " + std::to_string(w + 1) + " edges per product", first);
        part(out, "ArrayMultiplier<" + std::to_string(w) + ">", families::arrayMultiplier(w), true, "one combinational pass", first);
    }
    part(out, "MulDivUnit<32>", families::mulDivUnit(32), false, "multiply 33 edges, divide 33 edges", first);
    part(out, "ArrayMulDivUnit<32>", families::arrayMulDivUnit(32), false, "multiply 2 edges, divide 33 edges", first);
    part(out, "MacCell", families::MacCell, true, "int8 x int8 + int32, registered", first);
    part(out, "Requantizer", families::Requantizer, true, "int32 -> int8", first);
    // (The gate-level 16 x 16 array is not built here: about 200,000 gates.)
    for (size_t n : {2u, 4u, 8u})
        part(out, "SystolicArray<" + std::to_string(n) + ">", families::systolicArray(n), n <= 2, "weight-stationary", first);
    out << "\n]}\n";
    return 0;
}

std::string json(const std::string& s) {
    std::string r;
    for (char c : s) r += c == '\n' ? std::string("\\n") : c == '"' ? std::string("\\\"") : std::string(1, c);
    return r;
}

int program(std::map<std::string, std::string>& a) {
    runtime::AcceleratorRunOptions o;
    o.matrix = std::stoul(a["--matrix"]);
    o.memory = a["--memory"];
    o.period = std::stoull(a["--period"]);
    o.array_multiplier = a.contains("--array-multiplier");
    o.pipelined_bus = a.contains("--pipelined-bus");
    if (a.contains("--stall-cycles")) o.stall_cycles = std::stoull(a["--stall-cycles"]);
    if (a.contains("--dram-retention")) o.dram_retention = std::stoull(a["--dram-retention"]);
    const auto start = std::chrono::steady_clock::now();
    o.progress = [start](uint64_t cycles, uint64_t instructions) {
        std::cerr << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << " s: "
                  << cycles << " cycles, " << instructions << " instructions\n";
    };
    const auto name = a["--program"];
    const auto r = runtime::runAcceleratorGuest(runtime::acceleratorFixture(name), o);
    std::cout << "{\"program\": \"" << name << "\", \"matrix\": " << o.matrix << ", \"memory\": \"" << o.memory
              << "\", \"array_multiplier\": " << (o.array_multiplier ? "true" : "false")
              << ", \"pipelined_bus\": " << (o.pipelined_bus ? "true" : "false") << ", \"dram_retention\": " << o.dram_retention
              << ", \"profile\": \"" << timing::activeTimingProfile().name << "\", \"period\": " << o.period
              << ", \"halted\": " << (r.halted ? "true" : "false") << ", \"cycles\": " << r.cycles
              << ", \"instructions\": " << r.instructions << ", \"status\": " << r.word(3)
              << ", \"commands\": " << r.word(5) << ", \"sections\": [";
    for (uint32_t s = 0; s < r.word(4); ++s) std::cout << (s ? ", " : "") << r.word(6 + s);
    std::cout << "], \"outputs\": [";
    for (size_t i = 0; i < 64; ++i) std::cout << (i ? ", " : "") << int(int8_t(r.report[0x100 + i]));
    std::cout << "], \"setup_violations\": " << r.setup_violations << ", \"events\": " << r.events
              << ", \"setup_seconds\": " << r.setup_seconds << ", \"run_seconds\": " << r.run_seconds
              << ", \"console\": \"" << json(r.console) << "\"}\n";
    return r.halted && r.word(3) == 0 ? 0 : 2;
}
}

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> a{{"--matrix", "16"}, {"--memory", "sram"}, {"--period", "1000"}};
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--parts" || key == "--array-multiplier" || key == "--pipelined-bus") a[key] = "1";
            else if (i + 1 < argc) a[key] = argv[++i];
            else throw std::invalid_argument("Missing value for " + key);
        }
        if (a.contains("--parts")) return parts();
        if (a.contains("--program")) return program(a);
        throw std::invalid_argument("Usage: accelerator_experiment --parts | --program NAME [--matrix N] [--memory M] [--period T] [--array-multiplier] [--pipelined-bus] [--stall-cycles C] [--dram-retention TICKS]");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
