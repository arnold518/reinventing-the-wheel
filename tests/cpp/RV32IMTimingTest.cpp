#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "components/IOComponent.hpp"
#include <iostream>
#include <stdexcept>

// A standalone-core boundary certificate. Device/cache integration has its own
// timing graph and must be certified separately when that system is assembled.
int main() {
    using namespace circuit::analysis;
    try {
        for (const auto& profile : {"unit", "embedded", "desktop"}) {
            circuit::timing::setActiveTimingProfile(profile);
            for (const auto& contract : {"arithmetic.iterative.mul-div.32", "rv32im.core.educational-five-stage"}) {
                auto root = std::dynamic_pointer_cast<IOComponent>(circuit::builtinComponentCatalog().createRoot(
                    {contract,"DUT",{},{},{},{}},circuit::strictAllStructural()).root);
                ClockSpecification clock;
                clock.clock_pin = "DUT.CLK";
                for (const auto& [name,pin] : root->getAllInputPins()) if (name != "CLK") {
                    ClockInputConstraint input;
                    input.pin = "DUT." + name;
                    input.binary_values = true;
                    if (name == "RST") input.fixed_value = 0;
                    else {input.edge = ClockEdge::Falling; input.delay = {1,1};}
                    clock.inputs.push_back(input);
                }
                for (const auto& [name,pin] : root->getAllOutputPins())
                    clock.outputs.push_back({"DUT."+name,ClockEdge::Rising,0,0});
                auto report = CircuitTimingAnalyzer::safeClock(root,clock);
                for (const auto& message : report.diagnostics) std::cerr << message << '\n';
                if (report.status != ClockTimingStatus::SafeUnderContracts || !report.safe_period_ticks ||
                    !report.checked_setup_relationships || !report.checked_hold_relationships)
                    throw std::runtime_error(std::string("Uncertified M timing: ")+profile+" / "+contract);
                if (std::string(profile)=="unit" && *report.safe_period_ticks>100)
                    throw std::runtime_error("Unit core verification clock no longer covers the certificate");
                std::cout << profile << '\t' << contract << '\t' << *report.safe_period_ticks << '\t'
                          << report.node_count << '\t' << report.capture_count << '\t'
                          << report.checked_setup_relationships << '\t' << report.checked_hold_relationships << '\n';
                std::cout << "limiting: " << report.limiting_constraint << '\n';
            }
        }
        circuit::timing::setActiveTimingProfile("unit");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
