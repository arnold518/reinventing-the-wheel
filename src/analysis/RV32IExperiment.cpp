#include "analysis/RV32IExperiment.hpp"
#include "analysis/ExperimentClock.hpp"
#include "analysis/RV32IWorkloads.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/capabilities/RV32IRetirementView.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/rv32i/RV32IBaselinePipeline.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "rv32i/RV32IPipelineObserver.hpp"
#include "rv32i/RV32IFunctionalMemory.hpp"
#include "rv32i/RV32IInstructionOracle.hpp"
#include "rv32i/RV32IValidationConfig.hpp"
#include "simulator/SimulationTest.hpp"
#include <chrono>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace circuit::analysis {
namespace {
using Clock=std::chrono::steady_clock;
double seconds(Clock::time_point a,Clock::time_point b) {return std::chrono::duration<double>(b-a).count();}
void require(bool b,const std::string& s) {if(!b)throw std::runtime_error(s);}
uint64_t binaryTraceValue(const std::shared_ptr<PinBase>& pin,size_t cycle,uint64_t mask=UINT64_MAX) {
    const auto bits=pin->getValueAsVector();
    require(bits.size()<=64,"Cycle trace signal exceeds 64 bits: "+pin->getID());
    uint64_t value=0;
    for(size_t i=0;i<bits.size();++i) {
        if(!(mask&(uint64_t{1}<<i)))continue;
        require(bits[i]==LogicValue::LOW||bits[i]==LogicValue::HIGH,
            "Nonbinary active trace signal: "+pin->getID()+" cycle "+std::to_string(cycle));
        if(bits[i]==LogicValue::HIGH)value|=uint64_t{1}<<i;
    }
    return value;
}
void drive(Simulator& sim,size_t t,const std::shared_ptr<Wire<>>& wire,bool value) {
    sim.scheduleEvent(std::make_shared<WireUpdateEvent<>>(t,wire,value?LogicValue::HIGH:LogicValue::LOW));
}
void compare(const rv32i::RV32IState& a,const rv32i::RV32IState& b,size_t attempt) {
    require(a.pc==b.pc && a.x==b.x && a.halted==b.halted && a.trapped==b.trapped && a.trap_cause==b.trap_cause,
        "Architectural oracle mismatch at attempt "+std::to_string(attempt)+" PC="+std::to_string(a.pc)+" expected="+std::to_string(b.pc));
}
}
void RV32IExperimentConfig::validate() const {
    require(core=="single"||core=="baseline"||core=="advanced","Unknown core");
    require(fidelity=="behavioral"||fidelity=="reference","Unknown fidelity");
    ExperimentClock clock(period);
    (void)clock.edge(max_cycles);
    require(max_cycles && max_events,"Budgets must be positive");
    require(!trace_cycles || (max_cycles<=4096 && !timing_only && !safe_clock_only),
        "Cycle tracing requires execution with --max-cycles <= 4096");
    require(!progress_interval_cycles||bool(progress),"Progress interval requires a callback");
    require(program>=1&&program<=rv32i::validation::ProgramCount,"Program number outside shared corpus");
    prediction.validate();
    require(core=="advanced" || (prediction.policy==rv32i::PredictionPolicy::NotTaken && prediction.entries==16),
        "Prediction settings apply only to advanced");
    require(core=="advanced" || memory.technology==circuit::memory::Technology::FF,
        "Single-cycle and baseline require zero-wait FF endpoints");
    require(!timing_only || fidelity=="reference","Timing requires the reference structural profile, not a collapsed core");
    require(!safe_clock_only || fidelity=="reference","Safe-clock analysis requires the reference structural profile");
    require(!(safe_clock_only && timing_only),"Choose safe-clock analysis or legacy timing estimates, not both");
    require(!binary_startup || safe_clock_only,"Binary startup requires safe-clock analysis");
    memory.device.validate(32,memory.technology);
    require(memory.device.depth_words<=65536,"Experiment memory is limited to 256 KiB for bounded oracle checking");
    require((tohost&3)==0,"tohost must be word aligned");
    require(workload.empty()||isRV32IGeneratedWorkload(workload),"Unknown generated workload");
    if(isRV32IBenchmark(workload))validateRV32IBenchmark(workload,iterations,benchmark);
    else require(benchmark==RV32IBenchmarkOptions{},"Benchmark options require a new named benchmark");
    require(!benchmark.measure_body||(!start_pc&&!stop_pc),"Choose --measure-body or explicit PC markers");
    require(workload.empty()||elf.empty(),"Choose workload or ELF, not both");
    require(iterations>0&&iterations<=1000000,"Iterations must be 1..1000000");
    require(!stop_pc||start_pc,"stop-pc requires start-pc");
    require(!start_pc||(*start_pc%4==0),"Unaligned start-pc");
    require(!stop_pc||(*stop_pc%4==0 && stop_pc!=start_pc),"Invalid stop-pc");
}

RV32IExperimentResult runRV32IExperiment(const RV32IExperimentConfig& config) {
    config.validate();
    RV32IExperimentResult result;
    const auto started=Clock::now();
    Simulator sim;
    sim.setHistoryRecordingEnabled(false);
    const auto& catalog=circuit::builtinComponentCatalog();
    auto profile=config.fidelity=="behavioral"?circuit::strictAllBehavioral():
        rv32i::withBehavioralMemoryParts(circuit::maximallyStructural(),"rv32i-experiment-reference-v1");
    // Shared macro boundary: large zero-wait FF memory is compact for EVERY core.
    profile=circuit::withProfileOverrides(std::move(profile),{
        circuit::preferFidelity(circuit::Fidelity::Behavioral,circuit::ProfileSelector::exactPath("EXPERIMENT.IMEM"),"shared memory macro"),
        circuit::preferFidelity(circuit::Fidelity::Behavioral,circuit::ProfileSelector::exactPath("EXPERIMENT.DMEM"),"shared memory macro")});
    auto manifest=std::make_shared<circuit::BuildManifest>(profile.name(),profile.fingerprint());
    auto context=circuit::BuildContext::rootScope(catalog,std::make_shared<const circuit::BuildProfile>(profile),manifest)->child("EXPERIMENT",std::nullopt);
    auto root=Component::createWithContext<IOComponent>(context,"EXPERIMENT",[](IOComponent* s){
        s->addPin("CLK",PinType::INPUT);s->addPin("RST",PinType::INPUT);
    });
    ComponentBuilder b(root,context);
    const auto& family=config.core=="single"?circuit::families::RV32ISingleCycleCore:
        config.core=="baseline"?circuit::families::RV32IBaselinePipelineCore:circuit::families::RV32IFiveStageCore;
    auto core=b.add(family,"CORE",config.core=="advanced"?config.prediction.parameters():circuit::ParameterMap{});
    auto memory_config=config.memory;
    memory_config.device.record_history=false;
    auto imem=rv32i::buildMemory(b,*root,"IMEM",memory_config);
    auto dmem=rv32i::buildMemory(b,*root,"DMEM",memory_config);
    rv32i::wireMemorySystem(b,core,imem,dmem,root->getInputPin("CLK"),root->getInputPin("RST"));
    auto clk=b.addNewWire("CLOCK",nullptr,{root->getInputPin("CLK")});
    auto rst=b.addNewWire("RESET",nullptr,{root->getInputPin("RST")});
    auto enable=b.addNewWire("ENABLE",nullptr,{core->getInputPin("ENABLE")});
    result.manifest=manifest->serialize();
    if(config.safe_clock_only) {
        ClockSpecification clock;
        clock.binary_startup=config.binary_startup;
        if(config.binary_startup) {
            // The normal experiment preparation below uses this same zero-fill
            // policy. Any subsequently provisioned binary program/data bytes
            // preserve the property; the proof does not inspect their values.
            rv32i::memoryView(imem)->clearMemoryImage();
            rv32i::memoryView(dmem)->clearMemoryImage();
        }
        clock.clock_pin="EXPERIMENT.CLK";
        clock.inputs={{"EXPERIMENT.RST",std::nullopt,{},0},
                      {"EXPERIMENT.CORE.ENABLE",std::nullopt,{},1}};
        // Deliberately no state-dependent protocol cuts from the legacy timing
        // estimate. Unsupported contracts return Unknown, never a guessed clock.
        result.safe_clock=computeSafeClockPeriod(root,clock);
        result.setup_seconds=seconds(started,Clock::now());return result;
    }
    if(config.timing_only) {
        require(config.memory.technology==circuit::memory::Technology::FF,
            "Timing reference requires FF; delayed devices use the FF core clock with separate device ticks");
        // Analyze each transparent/held endpoint mode separately. The core
        // emits known-binary Read/Write requests, so opcode bit 0 cannot change
        // FF permission status. Read payload is timed only for an active read;
        // inactive/write payload is unobserved. This is a constrained timing
        // reference, NOT unrestricted four-state protocol static timing.
        for(unsigned mode=0;mode<4;++mode) {
            CircuitTimingOptions options;
            options.model=[mode](const IOComponent& c)->std::optional<ComponentTimingModel> {
                // In a valid access window ISSUE is asserted independently of
                // the eventual store commit. The redundant ISSUE qualification
                // on FAULT is a false path back from that commit.
                if(c.getID()=="EXPERIMENT.CORE.DATA_PORT.FAULT"
                    ||c.getID()=="EXPERIMENT.CORE.INSTRUCTION_PORT.FAULT")
                    return ComponentTimingModel{{{"B","OUT"}},{},{},1,1};
                // A held response belongs to an already accepted request:
                // SENT=1 makes RESPONSE_ELIGIBLE independent of new acceptance.
                if((c.getID()=="EXPERIMENT.CORE.FETCH_REQUEST.RESPONSE_ELIGIBLE"&&(mode&1))
                    ||(c.getID()=="EXPERIMENT.CORE.DATA_REQUEST.RESPONSE_ELIGIBLE"&&(mode&2)))
                    return ComponentTimingModel{{},{},{"OUT"},1,1};
                if(c.getContractId()!="memory.scalable.ff.32")return std::nullopt;
                ComponentTimingModel m;
                for(const auto& [n,p]:c.getAllInputPins())if(n!="CLK"&&n!="RST")m.capture_inputs.push_back(n);
                for(const auto& [n,p]:c.getAllOutputPins())m.launch_outputs.push_back(n);
                const bool held=(mode&(c.getName()=="IMEM"?1:2))!=0;
                if(held) m.combinational_arcs={{"RSP_READY","REQ_READY"}};
                else m.combinational_arcs={{"REQ_VALID","RSP_VALID"},{"REQ_ADDR","RSP_READ_DATA"},{"REQ_ADDR","RSP_STATUS"}};
                return m;
            };
            auto timing=CircuitTimingAnalyzer::analyze(root,options);
            if(!timing.valid){result.timing=std::move(timing);break;}
            if(timing.critical_path_delay>result.timing.critical_path_delay)result.timing=std::move(timing);
        }
        result.timing.limitation+=" Reference constraints: known-binary legal Read/Write, active-read payload, stable reset; "
            "ISSUE=1 for access fault qualification, SENT=1 for a held requester response; "
            "maximum across four empty/held FF endpoint modes. Clock must additionally pass dynamic oracle validation.";
        result.setup_seconds=seconds(started,Clock::now());return result;
    }
    auto iv=rv32i::memoryView(imem),dv=rv32i::memoryView(dmem);
    iv->setMemoryHistoryRecordingEnabled(false);dv->setMemoryHistoryRecordingEnabled(false);
    iv->clearMemoryImage();dv->clearMemoryImage();
    const size_t capacity=memory_config.device.depth_words*4;
    rv32i::RV32IFunctionalMemory oracle_imem(capacity),oracle_dmem(capacity);
    rv32i::RV32IState oracle;
    auto load=[&](bool instruction,uint32_t address,const std::vector<uint8_t>& bytes) {
        (instruction?iv:dv)->initializeMemoryBytes(address,bytes);
        (instruction?oracle_imem:oracle_dmem).loadBytes(address,bytes);
    };
    const auto prepared=prepareRV32IWorkload({config.program,config.workload,config.elf,
        config.iterations,config.benchmark,config.start_pc,config.stop_pc,config.tohost,capacity});
    for(const auto& block:prepared.blocks)load(block.instruction,block.address,block.bytes);
    const auto start_pc=prepared.measurement_start_pc,stop_pc=prepared.measurement_stop_pc;
    result.workload_description=prepared.descriptor.description;
    result.benchmark_expected_result=prepared.expected_result;
    result.benchmark_branch_pcs=prepared.branch_pcs;
    result.workload_fingerprint=prepared.fingerprint;
    result.measurement_start_pc=start_pc;result.measurement_stop_pc=stop_pc;
    auto state=std::dynamic_pointer_cast<RV32IStateView>(core);
    std::shared_ptr<RV32IPipelineObserver> observer;
    auto retirement=std::dynamic_pointer_cast<RV32IRetirementView>(core);
    if(config.core!="single") {
        observer=std::make_shared<RV32IPipelineObserver>(std::dynamic_pointer_cast<RV32IPipelineStateView>(core));
        state=observer;require(bool(retirement),"Missing retirement view");
    }
    require(bool(state),"Missing architectural state view");
    auto high=[&](const std::string& name) {
        auto it=core->getAllOutputPins().find(name);
        return it!=core->getAllOutputPins().end() && it->second->getValueAsUInt64()==1;
    };
    // Event guard includes reset; statistics below exclude it on every core.
    size_t consumed=0;
    auto advance=[&](size_t target) {
        auto drained=sim.drainUntilIdle(target,config.max_events-consumed);
        consumed+=drained.processed_events;
        require(drained.status!=DrainStatus::EventLimitReached,"Experiment event budget exceeded");
    };
    ExperimentClock clock(config.period);
    SimulationTest::scheduleInitialEventsForTree(root,sim,0);
    drive(sim,0,clk,false);drive(sim,0,rst,true);drive(sim,0,enable,true);
    advance(clock.resetRelease()-1);
    drive(sim,clock.resetRelease(),rst,false);advance(clock.origin());
    sim.resetPerformanceCounters();
    const auto execution_started=Clock::now();
    result.setup_seconds=seconds(started,execution_started);
    bool done=false;
    bool measuring=!start_pc,window_started=measuring,window_stopped=false;
    size_t start_visits=0;
    size_t total_attempts=0;
    bool instruction_pending=false,data_pending=false;
    for(size_t cycle=0;cycle<config.max_cycles;++cycle) {
        if(config.progress_interval_cycles&&cycle&&cycle%config.progress_interval_cycles==0)
            config.progress({cycle,total_attempts,result.retired,seconds(execution_started,Clock::now())});
        const auto cycle_started=Clock::now();
        const bool attempt=high(config.core=="single"?"INSTRUCTION_ATTEMPT":"COMMIT_VALID");
        RV32IRetirementObservation packet;
        if(attempt&&observer)packet=retirement->pendingRetirement();
        const auto pc=attempt?(observer?packet.pc:uint32_t(core->getOutputPin<32>("PC")->getValueAsUInt64())):0;
        if(config.trace_cycles) {
            auto& trace=result.cycle_trace.emplace_back();
            trace["cycle"]=cycle;
            trace["retire.valid"]=binaryTraceValue(core->getOutputPin(
                config.core=="single"?"INSTRUCTION_ATTEMPT":"COMMIT_VALID"),cycle);
            if(attempt) {
                trace["retire.pc"]=observer?pc:binaryTraceValue(core->getOutputPin<32>("PC"),cycle);
                trace["retire.instruction"]=observer?packet.instruction:
                    (binaryTraceValue(core->getInputPin<3>("IMEM_RSP_STATUS"),cycle)==0
                        ? binaryTraceValue(core->getInputPin<32>("IMEM_RSP_READ_DATA"),cycle):0);
            }
            for(auto name:{"PIPELINE_STALL","LOAD_USE_STALL","MEMORY_STALL","DATA_PORT_STALL",
                          "PIPELINE_FLUSH","DATA_STALL","BRANCH_WAIT","BRANCH_RESOLVED",
                          "BRANCH_MISS","PREDICTION_RECOVERY"}) {
                const auto pin=core->getAllOutputPins().find(name);
                if(pin!=core->getAllOutputPins().end())trace[name]=binaryTraceValue(pin->second,cycle);
            }
            for(const std::string stage:{"IF_ID","ID_EX","EX_MEM","MEM_WB"}) {
                const auto valid=core->getAllOutputPins().find(stage+"_VALID");
                if(valid==core->getAllOutputPins().end())continue;
                trace[stage+"_VALID"]=binaryTraceValue(valid->second,cycle);
                if(trace[stage+"_VALID"])
                    trace[stage+"_PC"]=binaryTraceValue(core->getOutputPin<32>(stage+"_PC"),cycle);
            }
            for(const std::string prefix:{"IMEM_","DMEM_"}) {
                const auto read=[&](const char* pin,bool input,uint64_t mask=UINT64_MAX) {
                    const auto p=input?core->getInputPinDynamic(prefix+pin):core->getOutputPinDynamic(prefix+pin);
                    return binaryTraceValue(p,cycle,mask);
                };
                for(auto pin:{"REQ_VALID","RSP_READY"})trace[prefix+pin]=read(pin,false);
                for(auto pin:{"REQ_READY","RSP_VALID"})trace[prefix+pin]=read(pin,true);
                // Inactive payload and unselected store lanes have no contract.
                if(trace[prefix+"REQ_VALID"]) {
                    const auto op=read("REQ_OP",false);
                    trace[prefix+"REQ_OP"]=op;trace[prefix+"REQ_ADDR"]=read("REQ_ADDR",false);
                    if(op==1) {
                        const auto mask=read("REQ_WRITE_MASK",false);
                        uint32_t lanes=0;for(unsigned i=0;i<4;++i)if(mask&(1u<<i))lanes|=0xffu<<(8*i);
                        trace[prefix+"REQ_WRITE_MASK"]=mask;
                        trace[prefix+"REQ_WRITE_DATA"]=read("REQ_WRITE_DATA",false,lanes);
                    }
                }
                if(trace[prefix+"RSP_VALID"]) {
                    trace[prefix+"RSP_STATUS"]=read("RSP_STATUS",true);
                    if(trace[prefix+"RSP_STATUS"]==0)trace[prefix+"RSP_READ_DATA"]=read("RSP_READ_DATA",true);
                }
            }
        }
        if(attempt&&!window_started&&start_pc&&pc==*start_pc
            &&start_visits++>=prepared.training_iterations) {
            measuring=window_started=true;sim.resetPerformanceCounters();
        }
        if(attempt&&measuring&&stop_pc&&pc==*stop_pc) {
            measuring=false;window_stopped=true;result.simulator=sim.getPerformanceCounters();
        }
        // Observe both ports even before the ROI so its first cycle inherits
        // the correct outstanding-request state. Zero-wait transfers never
        // become pending. These port waits are NOT additive CPI attribution.
        const auto observePort=[&](const std::string& prefix,bool& pending) {
            const bool valid=high(prefix+"REQ_VALID"),ready=high(prefix+"RSP_READY");
            const bool request_ready=core->getInputPin(prefix+"REQ_READY")->getValue()==LogicValue::HIGH;
            const bool response_valid=core->getInputPin(prefix+"RSP_VALID")->getValue()==LogicValue::HIGH;
            if(measuring) {
                result.observations[prefix+"REQUEST_BACKPRESSURE"]+=valid&&!request_ready;
                result.observations[prefix+"RESPONSE_WAIT"]+=(pending||(valid&&request_ready))&&!response_valid;
            }
            const bool accept=valid&&request_ready,complete=response_valid&&ready;
            pending=(pending&&(!complete||accept))||(!pending&&accept&&!complete);
        };
        observePort("IMEM_",instruction_pending);observePort("DMEM_",data_pending);
        if(measuring) {
        for(auto name:{"LOAD_USE_STALL","MEMORY_STALL","DATA_PORT_STALL","PIPELINE_FLUSH",
                      "DATA_STALL","BRANCH_WAIT","BRANCH_RESOLVED","BRANCH_MISS","PREDICTION_RECOVERY",
                      "IF_ID_VALID","ID_EX_VALID","EX_MEM_VALID","MEM_WB_VALID"})
            result.observations[name]+=high(name);
        }
        if(attempt&&observer) observer->retire(packet);
        drive(sim,clock.edge(cycle),clk,true);drive(sim,clock.falling(cycle),clk,false);
        advance(clock.end(cycle));++result.whole_program_cycles;
        if(config.trace_cycles) {
            auto& trace=result.cycle_trace.back();
            trace["post.halted"]=binaryTraceValue(core->getOutputPin("HALTED"),cycle);
            trace["post.trapped"]=binaryTraceValue(core->getOutputPin("TRAPPED"),cycle);
            trace["post.trap_cause"]=binaryTraceValue(core->getOutputPin<4>("TRAP_CAUSE"),cycle);
            if(attempt) {
                const auto architectural=state->snapshotArchitecturalState().toKnownState();
                trace["post.pc"]=architectural.pc;
                for(size_t i=0;i<architectural.x.size();++i)trace["post.x"+std::to_string(i)]=architectural.x[i];
            }
        }
        if(measuring)++result.cycles;
        if(!attempt) {
            if(measuring)result.execution_seconds+=seconds(cycle_started,Clock::now());
            continue;
        }
        ++total_attempts;
        if(measuring)++result.attempts;
        const bool terminal=high("HALTED")||high("TRAPPED");
        if(measuring){result.terminal_attempts+=terminal;result.retired+=!terminal;}
        double verification=0;
        if(config.verify) {
            const auto before=Clock::now();
            const auto trace=rv32i::RV32IInstructionOracle::step(oracle,oracle_imem,oracle_dmem);
            compare(state->snapshotArchitecturalState().toKnownState(),oracle,total_attempts);
            (void)trace;
            verification=seconds(before,Clock::now());
            result.verification_seconds+=verification;
        }
        if(prepared.completion==RV32IWorkloadCompletion::TerminalState) done=terminal;
        else {
            // A pipeline store is visible in MEM before its WB retirement.
            // Only the retired verdict store may end the benchmark.
            const bool verdict_retired=!observer ||
                (packet.memory.kind==rv32i::RV32IMemoryAccessKind::Write
                 && !packet.memory.fault && packet.memory.address==prepared.tohost);
            const auto bytes=dv->memoryStorage().readBytes(prepared.tohost,4);
            const uint32_t verdict=uint32_t(bytes[0])|(uint32_t(bytes[1])<<8)|(uint32_t(bytes[2])<<16)|(uint32_t(bytes[3])<<24);
            require(!verdict_retired||verdict!=3,"ELF self-check failed");
            done=verdict_retired&&verdict==1;
            require(done||!terminal,"ELF terminated before tohost");
        }
        if(measuring)result.execution_seconds+=seconds(cycle_started,Clock::now())-verification;
        if(done)break;
    }
    if(measuring)result.simulator=sim.getPerformanceCounters();
    result.completed=done;
    if(!done&&!config.allow_incomplete)require(false,"Experiment cycle budget exceeded");
    require(window_started,"Measurement start PC was never retired");
    require(!stop_pc||window_stopped||config.allow_incomplete,"Measurement stop PC was never retired");
    const auto verify_started=Clock::now();
    result.final_state=state->snapshotArchitecturalState().toKnownState();
    const auto actual_memory=dv->memoryStorage().readBytes(0,capacity);
    if(done&&prepared.expected_result)result.benchmark_result=result.final_state.x[10];
    if(config.verify) {
        // Compare the entire data image, catching wrong-path or duplicate stores,
        // including writes to locations not touched by the oracle.
        if(done)require(actual_memory==oracle_dmem.readBytes(0,capacity),"Final data memory image mismatch");
        result.prefix_verified=true;
        if(done&&prepared.expected_result) {
            require(result.final_state.halted&&!result.final_state.trapped,"Benchmark must halt normally");
            require(result.benchmark_result==result.benchmark_expected_result,"Independent benchmark result mismatch");
            for(const auto& data:prepared.expected_data)
                require(dv->memoryStorage().readBytes(data.address,data.bytes.size())==data.bytes,
                    "Independent benchmark output memory mismatch");
        }
        result.verified=done;
    }
    // Compare the final architectural result even on timed, oracle-free runs.
    uint64_t state_hash=14695981039346656037ULL;
    auto hash_byte=[&](uint8_t byte){state_hash^=byte;state_hash*=1099511628211ULL;};
    auto hash_word=[&](uint32_t word){for(unsigned n=0;n<4;++n)hash_byte(uint8_t(word>>(8*n)));};
    for(auto word:result.final_state.x)hash_word(word);
    hash_word(result.final_state.pc);hash_word(unsigned(result.final_state.trap_cause));
    hash_byte(result.final_state.halted);hash_byte(result.final_state.trapped);
    for(auto byte:actual_memory)hash_byte(byte);
    std::ostringstream state_digest;state_digest<<std::hex<<state_hash;result.state_fingerprint=state_digest.str();
    result.verification_seconds+=seconds(verify_started,Clock::now());
    result.benchmark_run.workload=prepared.descriptor;
    result.benchmark_run.target={"rv32i/"+config.core,"rv32i",{
        {"core",config.core},{"fidelity",config.fidelity},
        {"memory",circuit::memory::technologyName(config.memory.technology)},
        {"prediction",rv32i::predictionPolicyName(config.prediction.policy)},
        {"prediction_entries",std::to_string(config.prediction.entries)},
        {"period_ticks",std::to_string(config.period)}}};
    result.benchmark_run.status=done?benchmark::RunStatus::Completed:benchmark::RunStatus::Incomplete;
    result.benchmark_run.validated=result.verified;
    result.benchmark_run.simulated_ticks=result.cycles*config.period;
    result.benchmark_run.host_seconds=result.execution_seconds;
    benchmark::addMetric(result.benchmark_run,"cycles",double(result.cycles),"cycles");
    benchmark::addMetric(result.benchmark_run,"retired_instructions",double(result.retired),"instructions");
    benchmark::addMetric(result.benchmark_run,"processed_events",double(result.simulator.processed_events),"events");
    for(const auto& [name,value]:result.observations)
        benchmark::addMetric(result.benchmark_run,name,double(value),"cycles");
    result.benchmark_run.fingerprints={{"workload_fnv1a64",result.workload_fingerprint},
        {"final_state_fnv1a64",result.state_fingerprint}};
    if(config.progress_interval_cycles)
        config.progress({result.whole_program_cycles,total_attempts,result.retired,
            seconds(execution_started,Clock::now())});
    benchmark::validate(result.benchmark_run);
    return result;
}
}
