#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/rv32im/RV32IMDeviceSystem.hpp"
#include "runtime/RecordedGuestFixtures.hpp"
#include <iostream>
#include <stdexcept>
#include <set>

// The recorded fixtures use unit timing, a 1000-tick period and input changes
// 501 ticks after the preceding rising edge. Analyze that actual environment.
int main() {
    using namespace circuit::analysis;
    try {
        for (unsigned configuration=0;configuration<8;++configuration) {
            const bool expanded=configuration>=4,large=configuration%4==3;
            const unsigned memory=large?0:configuration%4;
            const bool caches=memory!=0,dram=memory==2;
            auto profile=circuit::BuildProfileBuilder("recorded-device-compact-children")
                .addRule(circuit::preferFidelity(circuit::Fidelity::Behavioral,circuit::ProfileSelector::any()))
                .unavailablePolicy(circuit::UnavailableFidelityPolicy::UseOnlyAvailableAndRecordException).build();
            if(expanded)profile=circuit::withProfileOverrides(std::move(profile),{
                circuit::preferFidelity(circuit::Fidelity::Structural,
                    circuit::ProfileSelector::contract("rv32im.core.educational-five-stage"),
                    "expand CPU assembly for recorded comparison")},"recorded-device-expanded-core");
            circuit::ParameterMap parameters{{"caches",caches?"1":"0"},{"dram",dram?"1":"0"}};
            if(large){parameters["width"]="128";parameters["height"]="96";parameters["console_depth"]="2";parameters["button_depth"]="7";}
            auto built=circuit::builtinComponentCatalog().createRoot(
                circuit::families::RV32IMDeviceSystem.request("DUT",parameters),profile);
            auto root=std::dynamic_pointer_cast<RV32IMDeviceSystem>(built.root);
            root->loadElf(rv32i::RV32IElfImage::fromBytes(
                circuit::runtime::recordedGuestFixture("bringup").elf));
            ClockSpecification clock;clock.clock_pin="DUT.CLK";
            for(const auto&[name,pin]:root->getAllInputPins())if(name!="CLK"){
                ClockInputConstraint input;input.pin="DUT."+name;input.binary_values=true;
                if(name=="RST") input.fixed_value=0;
                else if(name=="ENABLE"||name=="TX_READY") input.fixed_value=1;
                else {input.edge=ClockEdge::Rising;input.delay={501,501};}
                clock.inputs.push_back(input);
            }
            for(const auto&[name,pin]:root->getAllOutputPins())
                clock.outputs.push_back({"DUT."+name,ClockEdge::Rising,0,0});
            const auto report=CircuitTimingAnalyzer::safeClock(root,clock);
            for(const auto& line:report.diagnostics)std::cerr<<line<<'\n';
            std::cout<<(expanded?"expanded":"compact")<<'\t'<<(large?"128x96":"64x48")<<'\t'<<(dram?"cached-dram":caches?"cached-ff":"ff")<<'\t'<<clockTimingStatusName(report.status)
                <<'\t'<<report.safe_period_ticks.value_or(0)<<'\t'<<report.node_count<<'\t'
                <<report.checked_setup_relationships<<'\t'<<report.checked_hold_relationships<<'\n';
            std::cout<<"limiting: "<<report.limiting_constraint<<'\n';
            if(expanded) {
                const std::set<std::string> missing{
                    "DUT.CORE.FETCH: no all-state timing contract for RV32IFetchStage",
                    "DUT.CORE.IF_ID: no all-state timing contract for RV32IIFIDPipelineRegister",
                    "DUT.CORE.ID_EX: no all-state timing contract for RV32IIDEXPipelineRegister",
                    "DUT.CORE.EX_MEM: no all-state timing contract for RV32IEXMEMPipelineRegister",
                    "DUT.CORE.MEM_WB: no all-state timing contract for RV32IMEMWBPipelineRegister",
                    "DUT.CORE.PREDICTION: no all-state timing contract for RV32IPredictionPath"};
                if(report.status!=ClockTimingStatus::Unknown||report.safe_period_ticks||
                   std::set<std::string>(report.diagnostics.begin(),report.diagnostics.end())!=missing)
                    throw std::runtime_error("Expanded comparison profile's documented timing gap changed");
                std::cout<<"comparison-only: six missing contracts; no clock certificate\n";
                continue;
            }
            if(report.status!=ClockTimingStatus::SafeUnderContracts||!report.safe_period_ticks||
                *report.safe_period_ticks>1000||!report.checked_setup_relationships||!report.checked_hold_relationships)
                throw std::runtime_error("Recorded device-system clock is not certified");
        }
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
