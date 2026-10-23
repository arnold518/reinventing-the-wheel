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
uint32_t immediate(unsigned rd,unsigned rs1,int value,unsigned funct3=0) {
    return (uint32_t(value)&0xfff)<<20|(rs1<<15)|(funct3<<12)|(rd<<7)|0x13;
}
uint32_t integer(unsigned rd,unsigned rs1,unsigned rs2,unsigned funct3=0,unsigned funct7=0) {
    return (funct7<<25)|(rs2<<20)|(rs1<<15)|(funct3<<12)|(rd<<7)|0x33;
}
uint32_t store(unsigned value,unsigned base,unsigned offset=0) {
    return ((offset>>5)<<25)|(value<<20)|(base<<15)|(2<<12)|((offset&31)<<7)|0x23;
}
// Labels keep loop/branch targets tied to the actual instruction image.
class Assembly {
    struct Fixup { size_t index;std::string label;unsigned rs1,rs2,funct3;bool jump; };
    std::vector<uint32_t> words_;
    std::map<std::string,size_t> labels_;
    std::vector<Fixup> fixups_;
public:
    void emit(uint32_t word){words_.push_back(word);}
    void label(const std::string& name){require(labels_.emplace(name,words_.size()).second,"Duplicate assembly label");}
    void branch(const std::string& target,unsigned rs1,unsigned rs2,unsigned funct3=0) {
        fixups_.push_back({words_.size(),target,rs1,rs2,funct3,false});emit(0);
    }
    void jump(const std::string& target){fixups_.push_back({words_.size(),target,0,0,0,true});emit(0);}
    std::vector<uint32_t> finish() {
        for(const auto& f:fixups_) {
            const int32_t offset=4*(int32_t(labels_.at(f.label))-int32_t(f.index));
            require(offset%2==0&&offset>=(f.jump?-1048576:-4096)&&offset<=(f.jump?1048574:4094),"Assembly target out of range");
            const uint32_t bits=uint32_t(offset);
            words_[f.index]=f.jump?((bits>>20&1)<<31)|((bits>>1&1023)<<21)|((bits>>11&1)<<20)|(bits&0xff000)|0x6f:
                ((bits>>12&1)<<31)|((bits>>5&63)<<25)|(f.rs2<<20)|(f.rs1<<15)|(f.funct3<<12)|((bits>>1&15)<<8)|((bits>>11&1)<<7)|0x63;
        }
        return words_;
    }
};
struct Program {
    const char* test; const char* id;
    std::vector<uint32_t> words;
    std::map<unsigned,uint32_t> registers;
    std::map<uint32_t,uint32_t> memory;
    size_t max_cycles=1200;
    std::array<size_t,8> m_counts{};
};
Program fibonacci() {
    Assembly a;
    a.emit(addi(1,10)); a.emit(addi(5,2));
    a.emit(m(5,15,1,5));                    // half = n / 2
    a.emit(addi(2,1));
    a.label("highest-bit");
    a.branch("start",15,2,6);              // bltu half, mask
    a.emit(immediate(2,2,1,1)); a.jump("highest-bit");
    a.label("start");
    a.emit(addi(3,0)); a.emit(addi(4,1)); a.emit(addi(13,256));
    a.label("double");
    a.emit(immediate(6,4,1,1));
    a.emit(integer(6,6,3,0,0x20));         // 2*b - a
    a.emit(m(0,7,3,6));                    // c = a * (2*b-a)
    a.emit(m(0,9,3,3)); a.emit(m(0,10,4,4));
    a.emit(integer(11,9,10));              // d = a*a + b*b
    a.emit(integer(12,1,2,7));             // n & mask
    a.branch("even",12,0);
    a.emit(immediate(3,11,0)); a.emit(integer(4,7,11)); a.jump("record");
    a.label("even");
    a.emit(immediate(3,7,0)); a.emit(immediate(4,11,0));
    a.label("record");
    a.emit(store(3,13)); a.emit(store(4,13,4)); a.emit(immediate(13,13,8));
    a.emit(m(5,2,2,5));                    // mask /= 2
    a.branch("double",2,0,1); a.emit(0x00100073);
    Program p{"RV32IMFibonacciProgramTest","fibonacci",a.finish(),
        {{0,0},{1,10},{2,0},{3,55},{4,89},{13,288},{15,5}},
        {{256,1},{260,1},{264,1},{268,2},{272,5},{276,8},{280,55},{284,89}},2500};
    p.m_counts[0]=12;p.m_counts[5]=5;return p;
}
Program gcd() {
    Assembly a;
    a.emit(addi(1,1071));a.emit(addi(2,462));a.emit(addi(3,256));
    a.label("euclid"); a.branch("done",2,0);
    a.emit(m(7,4,1,2)); a.emit(m(5,5,1,2)); a.emit(m(0,6,5,2));
    a.emit(integer(6,6,4));                // q*b+r must equal a
    a.branch("failed",6,1,1);
    a.emit(store(4,3));a.emit(store(5,3,4));a.emit(immediate(3,3,8));
    a.emit(immediate(1,2,0));a.emit(immediate(2,4,0));a.jump("euclid");
    a.label("done");a.emit(store(1,3));a.emit(addi(10,0));a.emit(0x00100073);
    a.label("failed");a.emit(addi(10,1));a.emit(0x00100073);
    Program p{"RV32IMGcdProgramTest","gcd",a.finish(),
        {{0,0},{1,21},{2,0},{3,280},{4,0},{5,7},{6,147},{10,0}},
        {{256,147},{260,2},{264,21},{268,3},{272,0},{276,7},{280,21}},1800};
    p.m_counts[0]=3;p.m_counts[5]=3;p.m_counts[7]=3;return p;
}
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
    case 3:return fibonacci();
    case 4:return gcd();
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
    std::array<size_t,8> m_counts{};
    for(size_t cycle=1;cycle<=test.max_cycles;++cycle) {
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
            if(expected.m)++m_counts.at(size_t(*expected.m));
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
            for(size_t op=0;op<m_counts.size();++op)if(test.m_counts[op])
                require(m_counts[op]==test.m_counts[op],"Program did not execute the expected M operations");
            require(fixture_->readDataBytes(0,1024)==data.readBytes(0,1024),"Hardware final memory differs from oracle");
            require(BasicComponent::setupViolations()==0,"RV32IM program violated setup timing");
            duration_=edge+period_/2-1;
            checkpoints_.back().label="halted";
            checkpoints_.back().detail+="; Registers, memory and all commits match directed expectations.";
            precomputed_=true; return;
        }
    }
    throw std::runtime_error("Recorded RV32IM program exceeded "+std::to_string(test.max_cycles)+" cycles");
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
    for(size_t number=0;number<5;++number) {
        const auto p=programCase(number);
        entries.push_back({p.test,"RV32IMProgramTest",RegisteredTestKind::Program,{},"program-"+std::to_string(23+number),
            {"simulation","program","rv32im","recorded-program"},
            [number]{return std::make_unique<RV32IMProgramTest>(number);},true});
    }
}
