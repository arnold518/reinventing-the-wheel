#include "tests/RV32IMProgramTests.hpp"
#include "tests/RV32IProgramRoot.hpp"
#include "tests/TestRegistry.hpp"
#include "tests/TestHelpers.hpp"
#include "RV32IMScenarioProfile.hpp"
#include "modules/rv32im/RV32IMCore.hpp"
#include "rv32im/RV32IMInstruction.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/BuildManifest.hpp"
#include "components/TimingProfile.hpp"
#include <array>
#include <iomanip>
#include <sstream>

namespace {
void require(bool ok,const std::string& message) { if(!ok)throw std::runtime_error(message); }
uint32_t m(unsigned op,unsigned rd,unsigned rs1,unsigned rs2) {
    return 0x02000033u|(rs2<<20)|(rs1<<15)|(op<<12)|(rd<<7);
}
uint32_t addi(unsigned rd,int value) { return (uint32_t(value)&0xfff)<<20|(rd<<7)|0x13; }
struct Program {
    const char* test; const char* id;
    std::vector<uint32_t> words;
    std::map<unsigned,uint32_t> registers;
    std::map<uint32_t,uint32_t> memory;
};
Program programCase(size_t number) {
    switch(number) {
    case 0: {
        Program p{"RV32IMArithmeticProgramTest","arithmetic",{addi(1,-7),addi(2,3)},
            {{0,0},{1,0xfffffff9},{2,3},{3,0xffffffeb},{4,0xffffffff},{5,0xffffffff},
             {6,2},{7,0xfffffffe},{8,0x55555553},{9,0xffffffff},{10,0}}, {}};
        for(unsigned op=0;op<8;++op)p.words.push_back(m(op,3+op,1,2));
        p.words.push_back(m(0,0,1,2)); // M must still leave x0 unchanged.
        p.words.push_back(0x00100073); return p;
    }
    case 1:
        return {"RV32IMDependenciesProgramTest","dependencies",
            {addi(1,6),addi(2,7),addi(5,3),m(0,3,1,2),0x10302023,0x10002203,
             m(4,6,4,5),m(0,6,6,5),0x00330463,m(0,7,1,2),m(6,8,6,5),0x10602223,0x00100073},
            {{0,0},{3,42},{4,42},{6,42},{7,0},{8,0}},{{256,42},{260,42}}};
    case 2:
        return {"RV32IMDivideEdgesProgramTest","divide-edges",
            {0x800000b7,addi(2,-1),m(4,3,1,2),m(6,4,1,2),m(5,5,1,0),m(7,6,1,0),
             addi(7,-7),addi(8,3),m(4,9,7,8),m(6,10,7,8),m(1,11,1,2),m(2,12,1,2),0x00100073},
            {{0,0},{3,0x80000000},{4,0},{5,0xffffffff},{6,0x80000000},{9,0xfffffffe},
             {10,0xffffffff},{11,0},{12,0x80000000}}, {}};
    default: throw std::invalid_argument("Unknown RV32IM recorded program");
    }
}
rv32i::MemoryConfiguration memoryConfiguration() {
    auto memory=rv32i::MemoryConfiguration{};
    memory.device.depth_words=256;
    return memory;
}
class ProgramRoot : public RV32IProgramRoot {
public:
    ProgramRoot(std::string name,SimulationTime half_period)
        : RV32IProgramRoot(std::move(name),circuit::families::RV32IMFiveStageCore,
                          half_period,memoryConfiguration(),memoryConfiguration()) {}
    const char* getTypeName() const override { return "RV32IMProgramRoot"; }
};
}

RV32IMProgramTest::RV32IMProgramTest(size_t program)
    : program_(program),profile_(circuit::test::rv32imInspectionProfile()) { (void)programCase(program); }
std::string RV32IMProgramTest::getTestName()const { return programCase(program_).test; }
void RV32IMProgramTest::setBuildProfile(circuit::BuildProfile profile) {
    profile_=std::move(profile); root.reset(); builder.reset(); fixture_.reset(); core_.reset();
    sim=std::make_shared<Simulator>(); inputs_.clear(); checkpoints_.clear();
    initial_events_scheduled_=precomputed_=false; duration_=commits_=cycles_=arithmetic_stalls_=0;
}
void RV32IMProgramTest::setupCircuit() {
    if(root)return;
    const auto& timing=circuit::timing::activeTimingProfile().name;
    period_=timing=="unit"?200:timing=="embedded"?8000:6000;
    auto profile=std::make_shared<const circuit::BuildProfile>(profile_);
    auto manifest=std::make_shared<circuit::BuildManifest>(profile->name(),profile->fingerprint());
    auto context=circuit::BuildContext::rootScope(circuit::builtinComponentCatalog(),profile,manifest)->child("RV32IM_PROGRAM_ROOT",std::nullopt);
    fixture_=Component::createWithContext<ProgramRoot>(context,"RV32IM_PROGRAM_ROOT",period_/2);
    root=fixture_; core_=fixture_->core(); builder=std::make_unique<ComponentBuilder>(root,context);
    for(auto&[name,pin]:fixture_->getAllInputPins())
        inputs_[name]=builder->addNewWireDynamic("IN_"+name,pin->getWidth(),nullptr,{pin});
    for(auto&[name,pin]:fixture_->getAllOutputPins())builder->addNewWireDynamic("OUT_"+name,pin->getWidth(),pin,{});
}
void RV32IMProgramTest::preparePrecomputedSimulation() {
    if(precomputed_)return;
    setupCircuit(); const auto test=programCase(program_);
    rv32i::RV32IFunctionalMemory instructions(1024),data(1024);
    instructions.loadWords(0,test.words);
    fixture_->clearInstructionMemory(); fixture_->clearDataMemory();
    fixture_->loadProgram(rv32i::RV32IProgram::fromWords(test.words));
    rv32i::RV32IState reference;
    const auto retirement=std::dynamic_pointer_cast<RV32IRetirementView>(core_);
    require(bool(retirement),"Missing RV32IM retirement observer");
    auto drive=[&](SimulationTime time,const std::string& name,uint64_t value) {
        sim->scheduleEvent(makeTestWireUpdate(time,inputs_.at(name),TestValue(value)));
    };
    scheduleInitialEvents(); fixture_->initializeFixedInputs(*sim,0);
    drive(0,"RST",1); drive(0,"ENABLE",1);
    BasicComponent::resetSetupViolations();
    const auto first_edge=period_/5;
    const auto stateDetail=[](uint32_t pc,bool halted) {
        std::ostringstream text;
        text<<"; pc_after=0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<pc
            <<"; halted="<<(halted?"true":"false")<<"; trapped=false";
        return text.str();
    };
    fixture_->startClock(*sim,first_edge);
    sim->advanceAndRecord(first_edge+1); drive(first_edge+1,"RST",0);
    sim->advanceAndRecord(first_edge+1);
    checkpoints_.push_back({first_edge+1,"reset", "Reset released; instruction memory contains the program."+stateDetail(0,false),0});
    bool recorded_wait=false;
    for(size_t cycle=1;cycle<=1200;++cycle) {
        const auto edge=first_edge+cycle*period_;
        sim->advanceAndRecord(edge-1);
        const bool commit=core_->getOutputPin("COMMIT_VALID")->getValue()==LogicValue::HIGH;
        const bool waiting=core_->getOutputPin("EXECUTION_STALL")->getValue()==LogicValue::HIGH;
        arithmetic_stalls_+=waiting; cycles_=cycle;
        const bool first_wait=waiting&&!recorded_wait;
        if(first_wait)recorded_wait=true;
        rv32im::InstructionTrace expected;
        if(commit) {
            const auto actual=retirement->pendingRetirement();
            expected=rv32im::step(reference,instructions,data);
            require(actual.pc==expected.base.pc_before&&actual.instruction==expected.base.raw_instruction,
                    "RV32IM retirement differs from oracle at PC "+std::to_string(expected.base.pc_before));
            require(actual.memory.kind==expected.base.memory.kind&&actual.memory.fault==expected.base.memory.fault,
                    "RV32IM retired memory operation differs from oracle");
            if(actual.memory.kind!=rv32i::RV32IMemoryAccessKind::None)
                require(actual.memory.address==expected.base.memory.address,"RV32IM retired memory address differs");
            fixture_->observeRetirementBeforeEdge(*sim,edge); ++commits_;
        }
        drive(edge+period_/2-1,"ENABLE",1);
        sim->advanceAndRecord(edge+period_/2-1);
        if(commit) {
            const auto actual=fixture_->snapshotArchitecturalState().toKnownState();
            require(actual.x==reference.x&&actual.pc==reference.pc&&actual.halted==reference.halted&&
                    actual.trapped==reference.trapped&&actual.trap_cause==reference.trap_cause,
                    "RV32IM architectural state differs after commit "+std::to_string(commits_));
            std::ostringstream detail;
            detail<<"PC 0x"<<std::hex<<expected.base.pc_before<<": ";
            if(expected.m)detail<<rv32im::name(*expected.m)<<" -> x"<<std::dec<<unsigned(expected.base.rd)<<" = 0x"<<std::hex<<expected.base.alu_result;
            else detail<<"instruction 0x"<<std::hex<<expected.base.raw_instruction;
            checkpoints_.push_back({edge+period_/2-1,"commit-"+std::to_string(commits_),
                detail.str()+stateDetail(reference.pc,reference.halted),checkpoints_.size()});
        } else if(first_wait) {
            checkpoints_.push_back({edge+period_/2-1,"arithmetic-wait",
                "The pipeline retains its packet while the M unit iterates."+stateDetail(reference.pc,false),checkpoints_.size()});
        }
        if(reference.halted||reference.trapped) {
            require(reference.halted&&!reference.trapped,"Recorded program did not halt cleanly");
            require(arithmetic_stalls_>0,"Recorded program never exercised M stalls");
            for(auto [reg,value]:test.registers)require(reference.x.at(reg)==value,"Independent expected register mismatch: x"+std::to_string(reg));
            for(auto [address,value]:test.memory)require(data.readU32(address)==value,"Independent expected store mismatch");
            require(fixture_->readDataBytes(0,1024)==data.readBytes(0,1024),"Hardware final memory differs from oracle");
            require(BasicComponent::setupViolations()==0,"RV32IM program violated setup timing");
            duration_=edge+period_/2-1;
            checkpoints_.back().label="halted";
            checkpoints_.back().detail+="; Registers, memory and all commits match directed expectations.";
            precomputed_=true; return;
        }
    }
    throw std::runtime_error("Recorded RV32IM program exceeded 1200 cycles");
}
bool RV32IMProgramTest::run() {
    try { setupCircuit();preparePrecomputedSimulation();
        std::cout<<"[PASS] "<<getTestName()<<": "<<commits_<<" commits, "<<cycles_<<" cycles, "<<arithmetic_stalls_<<" M wait cycles\n";
        return true;
    }catch(const std::exception& e){std::cerr<<"[FAIL] "<<getTestName()<<": "<<e.what()<<'\n';return false;}
}
std::vector<SimulationTest::PerformanceMetric> RV32IMProgramTest::getPerformanceMetrics()const {
    auto metrics=SimulationTest::getPerformanceMetrics();
    metrics.insert(metrics.begin(),{{"program.commits",double(commits_),"instructions","Oracle-checked instruction commits."},
        {"program.cycles",double(cycles_),"cycles","Cycles from reset to terminal commit."},
        {"pipeline.execution_stalls",double(arithmetic_stalls_),"cycles","Cycles waiting for multiply/divide execution."}});
    return metrics;
}
void appendRV32IMProgramTests(std::vector<TestRegistryEntry>& entries) {
    for(size_t number=0;number<3;++number) {
        const auto p=programCase(number);
        entries.push_back({p.test,"RV32IMProgramTest",RegisteredTestKind::Program,{},p.id,
            {"simulation","program","rv32im","recorded-program"},
            [number]{return std::make_unique<RV32IMProgramTest>(number);},true});
    }
}
