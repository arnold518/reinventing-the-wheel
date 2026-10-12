#include "components/BasicComponent.hpp"
#include "analysis/RV32IExperiment.hpp"
#include "analysis/ExperimentClock.hpp"
#include "analysis/RV32IWorkloads.hpp"
#include <charconv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <sys/resource.h>

using namespace circuit::analysis;
namespace {
size_t number(const std::string& s) {
    size_t n=0;size_t offset=s.starts_with("0x")?2:0;
    const auto r=std::from_chars(s.data()+offset,s.data()+s.size(),n,offset?16:10);
    if(s.empty()||r.ec!=std::errc{}||r.ptr!=s.data()+s.size())throw std::invalid_argument("Invalid integer: "+s);
    return n;
}
std::string quote(const std::string& s) {
    std::ostringstream o;o<<'"';
    for(unsigned char c:s) {
        if(c=='"'||c=='\\')o<<'\\'<<c;
        else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<unsigned(c)<<std::dec;
        else o<<c;
    }
    o<<'"';return o.str();
}
void usage() {
    std::cout<<"rv32i_experiment [--core single|baseline|advanced] [--program 1..22 | --elf FILE]\n"
        "  [--fidelity behavioral|reference] [--policy not-taken|taken|backward-taken|one-bit|two-bit]\n"
        "  [--entries N] [--period EVEN_TICKS|auto] [--safe-clock] [--no-verify]\n"
        "  [--binary-startup] (with --safe-clock: checked binary-initialized contract)\n"
        "  [--workload NAME] [--iterations N] [--list-workloads | --list-benchmark-catalog]\n"
        "  [--start-pc PC --stop-pc PC]\n"
        "  [--size 1..256] [--distance 1|2|4|8] [--seed N] [--branch-spacing WORDS]\n"
        "  [--pattern taken|not-taken|alternating|bursts|irregular]\n"
        "  [--measure-body] [--training-iterations N] (new named benchmarks only)\n"
        "  [--memory ff|sram|dram|rom|nor-flash] [--phase-ticks N] [--guard-ticks N]\n"
        "  [--max-cycles N] [--max-events N] [--allow-incomplete] [--progress-cycles N]\n"
        "  [--tohost ADDRESS] [--allocation-limit-mib N]\n"
        "  [--trace-cycles] (requires --max-cycles <= 4096; validation only)\n"
        "JSON on stdout; errors on stderr. One fresh, cold-predictor run. History is disabled.\n";
}
}
int main(int argc,char** argv) {
    try {
        RV32IExperimentConfig c;
        bool automatic=false;size_t rss_mib=1536;
        for(int i=1;i<argc;++i) {
            std::string key=argv[i];
            if(key=="--help"){usage();return 0;}
            if(key=="--list-workloads") {
                std::cout<<'[';
                bool first=true;
                for(const auto& name:rv32iGeneratedWorkloadNames()) {
                    if(!first)std::cout<<',';
                    first=false;std::cout<<quote(name);
                }
                std::cout<<"]\n";return 0;
            }
            if(key=="--list-benchmark-catalog") {
                std::cout<<'[';
                bool first=true;
                for(const auto& workload:rv32iWorkloadCatalog()) {
                    if(!first)std::cout<<',';
                    first=false;
                    std::cout<<circuit::benchmark::toJson(workload);
                }
                std::cout<<"]\n";return 0;
            }
            if(key=="--measure-body"){c.benchmark.measure_body=true;continue;}
            if(key=="--safe-clock"){c.safe_clock_only=true;c.fidelity="reference";continue;}
            if(key=="--binary-startup"){c.binary_startup=true;continue;}
            if(key=="--no-verify"){c.verify=false;continue;}
            if(key=="--allow-incomplete"){c.allow_incomplete=true;continue;}
            if(key=="--trace-cycles"){c.trace_cycles=true;continue;}
            if(++i>=argc)throw std::invalid_argument("Missing value for "+key);
            std::string value=argv[i];
            if(key=="--core")c.core=value;
            else if(key=="--program")c.program=number(value);
            else if(key=="--elf")c.elf=value;
            else if(key=="--workload")c.workload=value;
            else if(key=="--iterations")c.iterations=number(value);
            else if(key=="--size")c.benchmark.size=number(value);
            else if(key=="--distance")c.benchmark.distance=number(value);
            else if(key=="--branch-spacing")c.benchmark.branch_spacing=number(value);
            else if(key=="--training-iterations")c.benchmark.training_iterations=number(value);
            else if(key=="--pattern")c.benchmark.pattern=value;
            else if(key=="--seed") {
                auto n=number(value);if(n>UINT32_MAX)throw std::invalid_argument("Seed exceeds RV32");
                c.benchmark.seed=uint32_t(n);
            }
            else if(key=="--start-pc"||key=="--stop-pc"){
                auto n=number(value);if(n>UINT32_MAX)throw std::invalid_argument("Window PC exceeds RV32");
                (key=="--start-pc"?c.start_pc:c.stop_pc)=uint32_t(n);
            }
            else if(key=="--fidelity")c.fidelity=value;
            else if(key=="--policy")c.prediction.policy=rv32i::predictionConfig({{"prediction",value}}).policy;
            else if(key=="--entries")c.prediction.entries=number(value);
            else if(key=="--period"){automatic=value=="auto";if(!automatic)c.period=number(value);}
            else if(key=="--max-cycles")c.max_cycles=number(value);
            else if(key=="--max-events")c.max_events=number(value);
            else if(key=="--progress-cycles")c.progress_interval_cycles=number(value);
            else if(key=="--tohost"){auto n=number(value);if(n>UINT32_MAX)throw std::invalid_argument("tohost exceeds RV32");c.tohost=uint32_t(n);}
            else if(key=="--memory")c.memory.technology=circuit::memory::technologyFromName(value);
            else if(key=="--phase-ticks")c.memory.device.phase_ticks=number(value);
            else if(key=="--guard-ticks")c.memory.device.guard_ticks=number(value);
            else if(key=="--allocation-limit-mib")rss_mib=number(value);
            else throw std::invalid_argument("Unknown option: "+key);
        }
        // Address-space cap is a hard allocation guard, not a claim about RSS.
        if(rss_mib<128||rss_mib>4096)throw std::invalid_argument("Allocation limit must be 128..4096 MiB");
        struct rlimit limit{rss_mib*1024*1024,rss_mib*1024*1024};
        if(setrlimit(RLIMIT_AS,&limit))throw std::runtime_error("Could not set allocation limit");
        if(c.progress_interval_cycles) c.progress=[](const RV32IExperimentProgress& progress) {
            std::cerr<<"progress cycles="<<progress.cycles
                <<" attempts="<<progress.instruction_attempts
                <<" retired="<<progress.retired_instructions
                <<" host_seconds="<<progress.host_seconds<<'\n';
        };
        // --period auto: the sufficient period certified for the structural
        // reference of this core, used for every fidelity.
        ClockTimingReport automatic_clock;
        if(automatic&&!c.safe_clock_only) {
            auto reference=c;reference.safe_clock_only=true;reference.fidelity="reference";
            reference.trace_cycles=false;
            // The clock is certified for the system as built, memory included:
            // a delayed memory answers the core a little later after the edge
            // than the flip-flop memory does. Device times remain ticks.
            automatic_clock=runRV32IExperiment(reference).safe_clock;
            if(automatic_clock.status!=ClockTimingStatus::SafeUnderContracts||!automatic_clock.safe_period_ticks) {
                for(const auto& d:automatic_clock.diagnostics)std::cerr<<d<<'\n';
                if(c.memory.technology!=circuit::memory::Technology::FF)
                    throw std::runtime_error("No clock can be certified yet for a system with this memory; give an explicit --period "
                        "(the result reports setup_violations, which must be 0)");
                throw std::runtime_error("No safe clock could be established; no automatic period available");
            }
            c.period=*automatic_clock.safe_period_ticks;
        }
        BasicComponent::resetSetupViolations();
        const auto r=runRV32IExperiment(c);
        // A compact part saw an input change inside its setup time: at this
        // clock its gates might capture something else, so the run is not a
        // trustworthy model of the gate-level system. Memories that time
        // themselves answer at any moment of the cycle and can cause this at
        // any clock.
        const auto setup_violations=BasicComponent::setupViolations();
        if(setup_violations&&!c.safe_clock_only)
            std::cerr<<"warning: "<<setup_violations<<" input change(s) inside a compact part's setup time at period "
                <<c.period<<"; the compact form may differ from its gates at this clock\n";
        if(c.safe_clock_only) {
            const auto& s=r.safe_clock;
            auto strings=[](const std::vector<std::string>& values) {
                std::cout<<'[';bool first=true;
                for(const auto& value:values){if(!first)std::cout<<',';first=false;std::cout<<quote(value);}
                std::cout<<']';
            };
            std::cout<<"{\"schema\":1,\"core\":"<<quote(c.core)
                <<",\"policy\":"<<quote(rv32i::predictionPolicyName(c.prediction.policy))
                <<",\"entries\":"<<c.prediction.entries
                <<",\"memory\":"<<quote(circuit::memory::technologyName(c.memory.technology))
                <<",\"clock_basis\":\"safe-clock-contracts-v1\",\"analysis_scope\":"
                <<quote(c.binary_startup?"binary-initialized":"unrestricted-four-state")
                <<",\"safe_clock\":{\"status\":"
                <<quote(clockTimingStatusName(s.status))<<",\"safe_period_ticks\":";
            if(s.safe_period_ticks)std::cout<<*s.safe_period_ticks;else std::cout<<"null";
            std::cout<<",\"node_count\":"<<s.node_count<<",\"edge_count\":"<<s.edge_count
                <<",\"capture_count\":"<<s.capture_count
                <<",\"checked_setup_relationships\":"<<s.checked_setup_relationships
                <<",\"checked_hold_relationships\":"<<s.checked_hold_relationships
                <<",\"limiting_constraint\":"<<quote(s.limiting_constraint)<<",\"critical_path\":";
            strings(s.critical_path);std::cout<<",\"dependency_cycle\":";strings(s.dependency_cycle);
            std::cout<<",\"state_case_count\":"<<s.state_case_count<<",\"checked_state_cases\":"<<s.checked_state_cases
                <<",\"state_evaluations\":"<<s.state_evaluations
                <<",\"binary_startup_verified\":"<<(s.binary_startup_verified?"true":"false")
                <<",\"binary_capture_bits\":"<<s.binary_capture_bits
                <<",\"binary_evidence\":";strings(s.binary_evidence);
            std::cout
                <<",\"state_evidence\":";strings(s.state_evidence);
            std::cout<<",\"assumptions\":";strings(s.assumptions);
            std::cout<<",\"contracts\":";strings(s.contracts);
            std::cout<<",\"diagnostics\":";strings(s.diagnostics);
            std::cout<<"},\"manifest\":"<<quote(r.manifest)<<"}\n";
            return s.status==ClockTimingStatus::SafeUnderContracts?0:2;
        }
        struct rusage usage{};getrusage(RUSAGE_SELF,&usage);
        std::cout<<std::setprecision(12)<<"{\"schema\":1,\"core\":"<<quote(c.core)
            <<",\"program\":"<<c.program<<",\"elf\":"<<quote(c.elf)
            <<",\"fidelity\":"<<quote(c.fidelity)<<",\"policy\":"<<quote(rv32i::predictionPolicyName(c.prediction.policy))
            <<",\"entries\":"<<c.prediction.entries<<",\"period_ticks\":"<<c.period
            <<",\"memory\":"<<quote(circuit::memory::technologyName(c.memory.technology))
            <<",\"workload\":"<<quote(c.workload)<<",\"iterations\":"<<c.iterations
            <<",\"size\":"<<c.benchmark.size<<",\"distance\":"<<c.benchmark.distance
            <<",\"pattern\":"<<quote(c.benchmark.pattern)<<",\"branch_spacing\":"<<c.benchmark.branch_spacing
            <<",\"seed\":"<<c.benchmark.seed<<",\"training_iterations\":"<<c.benchmark.training_iterations
            <<",\"workload_description\":"<<quote(r.workload_description)
            <<",\"benchmark_expected_result\":"<<(r.benchmark_expected_result?std::to_string(*r.benchmark_expected_result):"null")
            <<",\"benchmark_result\":"<<(r.benchmark_result?std::to_string(*r.benchmark_result):"null")
            <<",\"phase_ticks\":"<<c.memory.device.phase_ticks<<",\"guard_ticks\":"<<c.memory.device.guard_ticks
            <<",\"history\":false,\"predictor_start\":\"cold\",\"window\":"<<quote(r.measurement_start_pc?"retirement-PC-region":"whole-program")
            <<",\"start_pc\":"<<(r.measurement_start_pc?std::to_string(*r.measurement_start_pc):"null")
            <<",\"stop_pc\":"<<(r.measurement_stop_pc?std::to_string(*r.measurement_stop_pc):"null")
            <<",\"predictor_at_window\":"<<quote(r.measurement_start_pc?"inherits-prefix-training":"cold")
            <<",\"completed\":"<<(r.completed?"true":"false")
            <<",\"prefix_verified\":"<<(r.prefix_verified?"true":"false")
            <<",\"verified\":"<<(r.verified?"true":"false")<<",\"setup_violations\":"<<setup_violations<<",\"cycles\":"<<r.cycles
            <<",\"instruction_attempts\":"<<r.attempts<<",\"retired_instructions\":"<<r.retired
            <<",\"terminal_attempts\":"<<r.terminal_attempts
            <<",\"whole_program_cycles\":"<<r.whole_program_cycles
            <<",\"cpi\":"<<(r.retired?double(r.cycles)/r.retired:0)
            <<",\"ipc\":"<<(r.cycles?double(r.retired)/r.cycles:0)
            <<",\"scheduled_time_ticks\":"<<r.cycles*c.period
            <<",\"clock_basis\":"<<quote(automatic?"reference-pin-arcs-v1":"explicit-scheduling-period")
            <<",\"modeled_time_ticks\":";
        if(automatic)std::cout<<r.cycles*c.period;else std::cout<<"null";
        std::cout<<",\"host_setup_seconds\":"<<r.setup_seconds
            <<",\"host_execution_seconds\":"<<r.execution_seconds
            <<",\"host_verification_seconds\":"<<r.verification_seconds
            <<",\"peak_rss_kib\":"<<usage.ru_maxrss
            <<",\"workload_fnv1a64\":"<<quote(r.workload_fingerprint)
            <<",\"final_state_fnv1a64\":"<<quote(r.state_fingerprint)
            <<",\"compiler\":"<<quote(__VERSION__)
            <<",\"processed_events\":"<<r.simulator.processed_events
            <<",\"component_evaluations\":"<<r.simulator.processed_component_evaluations
            <<",\"wire_updates\":"<<r.simulator.processed_wire_updates
            <<",\"effective_wire_changes\":"<<r.simulator.effective_wire_changes
            <<",\"maximum_queue_depth\":"<<r.simulator.maximum_event_queue_depth
            <<",\"average_pipeline_occupancy\":";
        if(c.core=="single"||!r.cycles)std::cout<<"null";
        else {
            uint64_t occupied=0;for(auto p:{"IF_ID_VALID","ID_EX_VALID","EX_MEM_VALID","MEM_WB_VALID"})occupied+=r.observations.at(p);
            std::cout<<double(occupied)/(4.0*r.cycles);
        }
        std::cout<<",\"benchmark_branch_pcs\":[";
        for(size_t i=0;i<r.benchmark_branch_pcs.size();++i){if(i)std::cout<<',';std::cout<<r.benchmark_branch_pcs[i];}
        std::cout<<"],\"observations\":{";
        bool first=true;for(const auto& [name,value]:r.observations){if(!first)std::cout<<',';first=false;std::cout<<quote(name)<<':'<<value;}
        std::cout<<"},\"timing\":";
        if(automatic) {
            std::cout<<"{\"guarantee\":\"safe-under-contracts\",\"period\":"<<*automatic_clock.safe_period_ticks
                <<",\"limiting_constraint\":"<<quote(automatic_clock.limiting_constraint)<<"}";
        } else std::cout<<"null";
        std::cout<<",\"benchmark_run\":";
        std::cout<<circuit::benchmark::toJson(r.benchmark_run);
        if(c.trace_cycles) {
            std::cout<<",\"cycle_trace_schema\":1,\"cycle_trace\":[";
            bool first_cycle=true;
            for(const auto& cycle:r.cycle_trace) {
                if(!first_cycle)std::cout<<',';
                first_cycle=false;std::cout<<'{';
                bool first_field=true;
                for(const auto& [name,value]:cycle) {
                    if(!first_field)std::cout<<',';
                    first_field=false;std::cout<<quote(name)<<':'<<value;
                }
                std::cout<<'}';
            }
            std::cout<<']';
        }
        std::cout<<",\"manifest\":"<<quote(r.manifest)<<"}\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"Experiment failed: "<<e.what()<<'\n';return 1;}
}
