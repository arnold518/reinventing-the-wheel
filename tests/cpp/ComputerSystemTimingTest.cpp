#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/TimingProfile.hpp"
#include "system/ComputerSystem.hpp"
#include "runtime/RecordedGuestFixtures.hpp"
#include <iostream>
#include <stdexcept>
#include <set>

// Certifies a safe clock for each machine in the compact profile the games
// run with, in the active timing profile. Inputs change one tick after the
// falling edge (the recorded runner's boundary). Prints one line per machine:
// machine, memory, status, safe period, nodes, setup and hold checks.
int main(int argc, char** argv) {
    using namespace circuit::analysis;
    try {
        const bool unit = circuit::timing::unitTiming();
        struct Case { std::string machine; circuit::ParameterMap options; std::string label; };
        std::vector<Case> cases{
            {"games-machine", {}, "sram"}, {"games-machine", {{"caches", "1"}}, "cached-sram"},
            {"games-machine", {{"memory", "ff"}}, "ff"}, {"games-machine", {{"caches", "1"}, {"memory", "ff"}}, "cached-ff"},
            {"games-machine", {{"memory", "dram"}}, "dram"}, {"games-machine", {{"caches", "1"}, {"memory", "dram"}}, "cached-dram"},
            {"games-machine", {{"width", "128"}, {"height", "96"}, {"console_depth", "2"}, {"button_depth", "7"}}, "sram-128x96"},
            {"games-machine-two-bus", {}, "sram"}, {"games-machine-registered-bridge", {}, "sram"}, {"games-machine-leds", {}, "sram"}};
        if (argc > 1) {  // one machine only
            std::vector<Case> chosen;
            for (const auto& c : cases) if (c.machine + ":" + c.label == argv[1]) chosen.push_back(c);
            if (chosen.empty()) throw std::invalid_argument("Unknown case; use machine:memory, e.g. games-machine:sram");
            cases = chosen;
        }
        for (const auto& c : cases) {
            auto profile = circuit::BuildProfileBuilder("recorded-device-compact-children")
                .addRule(circuit::preferFidelity(circuit::Fidelity::Behavioral, circuit::ProfileSelector::any()))
                .unavailablePolicy(circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException).build();
            auto parameters = c.options;
            parameters["machine"] = c.machine;
            // An SRAM or DRAM memory is set up for the clock the games run
            // at, as the recorded runner does: 4.5 ns in a realistic profile,
            // 1000 ticks in unit.
            if (!c.label.starts_with("ff") && !c.label.starts_with("cached-ff")) parameters["clock_period"] = unit ? "1000" : "4500";
            auto built = circuit::builtinComponentCatalog().createRoot({"system.computer", "DUT", parameters}, profile);
            auto root = std::dynamic_pointer_cast<circuit::system::ComputerSystem>(built.root);
            root->setMemoryHistoryRecordingEnabled(false);
            root->loadElf(rv32i::RV32IElfImage::fromBytes(circuit::runtime::recordedGuestFixture("bringup").elf));
            ClockSpecification clock;
            clock.clock_pin = "DUT.CLK";
            for (const auto& [name, pin] : root->getAllInputPins()) if (name != "CLK") {
                ClockInputConstraint input;
                input.pin = "DUT." + name;
                input.binary_values = true;
                if (name == "RST") input.fixed_value = 0;
                else if (name == "ENABLE" || name == "TX_READY") input.fixed_value = 1;
                else { input.edge = ClockEdge::Falling; input.delay = {1, 1}; }
                clock.inputs.push_back(input);
            }
            for (const auto& [name, pin] : root->getAllOutputPins()) clock.outputs.push_back({"DUT." + name, ClockEdge::Rising, 0, 0});
            const auto report = CircuitTimingAnalyzer::safeClock(root, clock);
            for (const auto& line : report.diagnostics) std::cerr << line << '\n';
            std::cout << c.machine << '\t' << c.label << '\t' << clockTimingStatusName(report.status) << '\t'
                      << report.safe_period_ticks.value_or(0) << '\t' << report.node_count << '\t'
                      << report.checked_setup_relationships << '\t' << report.checked_hold_relationships << '\n';
            std::cout << "limiting: " << report.limiting_constraint << '\n';
            if (report.status != ClockTimingStatus::SafeUnderContracts || !report.safe_period_ticks ||
                *report.safe_period_ticks > (unit ? 1000u : 4500u) || !report.checked_setup_relationships || !report.checked_hold_relationships)
                throw std::runtime_error("Machine clock is not certified (it must fit the period the games run at: 1000 ticks in unit, 4.5 ns otherwise)");
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
