// Audit-only binding of the existing experiment runner. No viewer changes.
#include "analysis/RV32IExperiment.hpp"
#include "components/BasicComponent.hpp"
#include "components/TimingProfile.hpp"
#include <emscripten/bind.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
struct Case { const char* name; uint32_t begin,end; size_t frames; };
const Case cases[]={
#include "audit_cases.inc"
};
std::string runAudit(const std::string& name,const std::string& memory,
                     const std::string& topology,const std::string& fidelity,bool verify) {
 const Case* selected=nullptr;
 for(const auto& c:cases)if(name==c.name)selected=&c;
 if(!selected)throw std::invalid_argument("Unknown audit case");
 circuit::timing::setActiveTimingProfile("embedded");
 circuit::analysis::RV32IExperimentConfig config;
 config.core="advanced";config.fidelity=fidelity;
 config.elf="/elfs/"+name+".elf";
 config.memory.technology=circuit::memory::technologyFromName(memory);
 config.memory.device.clocked=memory=="sram";
 config.topology=topology;config.period=4104;
 config.start_pc=selected->begin;config.stop_pc=selected->end;
 config.max_cycles=500000;config.max_events=3000000000ULL;config.verify=verify;
 BasicComponent::resetSetupViolations();
 const auto started=std::chrono::steady_clock::now();
 const auto r=circuit::analysis::runRV32IExperiment(config);
 const double wall=std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
 std::ostringstream out;
 out<<std::setprecision(12)<<"{\"completed\":"<<(r.completed?"true":"false")
  <<",\"verified\":"<<(r.verified?"true":"false")
  <<",\"setup_violations\":"<<BasicComponent::setupViolations()
  <<",\"frames\":"<<selected->frames<<",\"cycles\":"<<r.cycles
  <<",\"retired_instructions\":"<<r.retired
  <<",\"host_execution_seconds\":"<<r.execution_seconds
  <<",\"host_setup_seconds\":"<<r.setup_seconds
  <<",\"host_verification_seconds\":"<<r.verification_seconds
  <<",\"total_wall_seconds\":"<<wall
  <<",\"processed_events\":"<<r.simulator.processed_events
  <<",\"final_state_fnv1a64\":\""<<r.state_fingerprint<<"\""
  <<",\"fps\":"<<selected->frames/r.execution_seconds<<"}";
 return out.str();
}
}
EMSCRIPTEN_BINDINGS(playability_audit) {
 emscripten::function("runAudit",&runAudit);
}
