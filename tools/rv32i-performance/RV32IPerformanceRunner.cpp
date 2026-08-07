#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/Component.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "simulator/SimulationTest.hpp"
#include "tests/RV32IFiveStageTests.hpp"
#include "tests/RV32ISingleCycleTests.hpp"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

std::shared_ptr<Component> findContract(
    const std::shared_ptr<Component>& component,
    const std::string& contract) {
    if (!component) {
        return nullptr;
    }
    if (component->getContractId() == contract) {
        return component;
    }
    for (const auto& child : component->getChildren()) {
        if (auto found = findContract(child, contract)) {
            return found;
        }
    }
    return nullptr;
}

void printTiming(
    const std::string& scope,
    const circuit::analysis::CircuitTimingReport& report) {
    std::cout << "timing\t" << scope
              << "\tvalid=" << (report.valid ? 1 : 0)
              << "\tcritical_delay=" << report.critical_path_delay
              << "\tcombinational_components="
              << report.combinational_component_count
              << "\tsequential_boundaries="
              << report.sequential_boundary_count
              << "\tconnections=" << report.connection_count
              << "\tcycle="
              << (report.combinational_cycle_detected ? 1 : 0)
              << '\n';
    std::cout << "critical_path\t" << scope;
    const size_t path_size = report.critical_path.size();
    for (size_t index = 0; index < path_size; ++index) {
        if (path_size > 14 && index == 7) {
            std::cout << "\t...(" << (path_size - 14)
                      << " components omitted)...";
            index = path_size - 8;
            continue;
        }
        const auto& point = report.critical_path[index];
        std::cout << '\t' << point.component_id
                  << ':' << point.component_type
                  << '@' << point.arrival_time;
    }
    std::cout << '\n';
}

void usage(const char* executable) {
    std::cerr << "Usage: " << executable
              << " <single|five> <program-number> "
                 "<structural|behavioral>\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        usage(argv[0]);
        return 2;
    }
    const std::string architecture = argv[1];
    const size_t program = std::stoul(argv[2]);
    const std::string fidelity = argv[3];
    if (fidelity != "structural" && fidelity != "behavioral") {
        usage(argv[0]);
        return 2;
    }

    std::unique_ptr<SimulationTest> scenario;
    std::string core_contract;
    if (architecture == "single") {
        auto test = std::make_unique<RV32ISingleCycleSystemTest>(program);
        if (fidelity == "behavioral") {
            test->setBuildProfile(circuit::strictAllBehavioral());
        }
        core_contract = "rv32i.core.educational-single-cycle";
        scenario = std::move(test);
    } else if (architecture == "five") {
        auto test = std::make_unique<RV32IFiveStageCoreProgramTest>(program);
        if (fidelity == "behavioral") {
            test->setBuildProfile(circuit::strictAllBehavioral());
        }
        core_contract = "rv32i.core.educational-five-stage";
        scenario = std::move(test);
    } else {
        usage(argv[0]);
        return 2;
    }

    const auto started = std::chrono::steady_clock::now();
    const bool passed = scenario->SimulationTest::run();
    const auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - started).count();
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "run\tarchitecture=" << architecture
              << "\tprogram=" << program
              << "\tfidelity=" << fidelity
              << "\tpassed=" << (passed ? 1 : 0)
              << "\thost_seconds=" << elapsed << '\n';
    if (!passed) {
        return 1;
    }

    for (const auto& metric : scenario->getPerformanceMetrics()) {
        std::cout << "metric\t" << metric.name
                  << "\tvalue=" << metric.value
                  << "\tunit=" << metric.unit << '\n';
    }

    const auto core = findContract(scenario->getRoot(), core_contract);
    // The fully behavioral single-cycle system intentionally collapses the
    // system and core into one evaluated reference component.
    printTiming(
        core ? "core" : "collapsed-implementation",
        circuit::analysis::CircuitTimingAnalyzer::analyze(
            core ? core : scenario->getRoot()));
    printTiming(
        "scenario",
        circuit::analysis::CircuitTimingAnalyzer::analyze(
            scenario->getRoot()));
    if (architecture == "five" && core) {
        for (const auto& [scope, contract] : {
                 std::pair{"stage-fetch", "rv32i.pipeline.stage.fetch"},
                 std::pair{"stage-decode", "rv32i.pipeline.stage.decode"},
                 std::pair{"stage-execute", "rv32i.pipeline.stage.execute"},
                 std::pair{"stage-memory", "rv32i.pipeline.stage.memory"},
                 std::pair{"stage-writeback", "rv32i.pipeline.stage.writeback"},
             }) {
            if (const auto stage = findContract(core, contract)) {
                printTiming(
                    scope,
                    circuit::analysis::CircuitTimingAnalyzer::analyze(stage));
            }
        }
    }
    return 0;
}
