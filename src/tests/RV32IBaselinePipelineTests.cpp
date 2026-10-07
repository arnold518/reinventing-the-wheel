#include "tests/RV32IBaselinePipelineTests.hpp"
#include "tests/RV32IFiveStageTests.hpp"
#include "tests/RV32IProgramCases.hpp"
#include "modules/rv32i/RV32IBaselinePipeline.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/rv32i/RV32IPipelineEncoding.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "components/Component.tpp"

using namespace circuit;
using namespace circuit::test;
namespace {
NamedValues vals(std::initializer_list<std::pair<std::string,uint32_t>> v,const ComponentFamily& f,bool outputs=false) {
    auto pins=Component::create<IOComponent>("SCHEMA",f.pinInitializer());NamedValues r;
    for(auto& [p,x]:v) r[p]=logicBits((outputs?pins->getOutputPinDynamic(p):pins->getInputPinDynamic(p))->getWidth(),x);
    return r;
}
struct Scenario : ActionScenario {
    const ComponentFamily& f;
    explicit Scenario(const ComponentFamily& family):ActionScenario{"contract",{},{},{500000,10000000}},f(family) {
        auto pins=Component::create<IOComponent>("SCHEMA",f.pinInitializer());NamedValues zero;
        for(auto& [p,v]:pins->getAllInputPins()) zero[p]=logicBits(v->getWidth(),0);
        if(pins->getAllInputPins().contains("RST")) zero["RST"]=logicBit(true);
        actions.push_back({"initial-inputs",CheckpointKind::Settled,zero,{}});
    }
    void add(std::string label,std::initializer_list<std::pair<std::string,uint32_t>> in,
            std::initializer_list<std::pair<std::string,uint32_t>> out={}) {
        actions.push_back({std::move(label),CheckpointKind::Settled,vals(in,f),vals(out,f,true)});
    }
    void edge(std::string label,std::initializer_list<std::pair<std::string,uint32_t>> out={}) {
        actions.push_back({std::move(label),CheckpointKind::AfterEdge,{{"CLK",logicBit(true)}},vals(out,f,true)});
    }
    ComponentTestSpec spec() const {return {std::string(f.typeName())+"Test",std::string(f.id()),std::string(f.typeName())+"_ROOT",{},{*this}};}
};
uint32_t controlWord(uint32_t instruction) {return rv32i::pipeline_encoding::packControl(rv32i::RV32IControl::fromDecoded(rv32i::RV32IDecoder::decode(instruction)));}
ComponentTestSpec fetchSpec() {
    Scenario s(families::RV32IBaselineFetchStage);
    s.add("reset",{{"RST",1}},{{"FETCH_PC",0},{"OUT_VALID",0}});
    s.add("read-before-edge",{{"RST",0},{"FETCH_VALID",1},{"PC_WRITE",1},{"IMEM_READ_DATA",0x00500093}},{{"OUT_PC",0},{"OUT_INSTRUCTION",0x00500093},{"IMEM_WORD_ADDR",0},{"OUT_VALID",1}});
    s.edge("advance",{{"FETCH_PC",4},{"IMEM_WORD_ADDR",1}});
    s.add("hold-setup",{{"CLK",0},{"PC_WRITE",0},{"FETCH_VALID",0}});s.edge("hold",{{"FETCH_PC",4},{"OUT_VALID",0}});
    s.add("redirect-setup",{{"CLK",0},{"PC_WRITE",1},{"PC_REDIRECT",1},{"REDIRECT_PC",0x102}});
    s.edge("redirect-with-alignment-status",{{"FETCH_PC",0x102},{"OUT_FETCH_STATUS",1},{"IMEM_WORD_ADDR",0x40}});
    s.add("fetch-fault",{{"IMEM_FAULT",1}},{{"OUT_FETCH_STATUS",3}});
    s.add("reset-again",{{"RST",1},{"IMEM_FAULT",0}},{{"FETCH_PC",0},{"OUT_FETCH_STATUS",0}});return s.spec();
}
ComponentTestSpec decodeSpec() {
    Scenario s(families::RV32IBaselineDecodeStage);s.add("reset",{{"RST",1}});
    s.add("independent-write-destination",{{"RST",0},{"IN_VALID",1},{"IN_INSTRUCTION",0x002083b3},{"WB_RD",1},{"WB_VALUE",7},{"REGISTER_WRITE",1}},
        {{"OUT_RS1_VALUE",0},{"OUT_REGISTER_ADDRESSES",1|(2<<5)|(7<<10)},{"TERMINAL",0}});
    s.edge("write-x1-not-decoded-x7",{{"OUT_RS1_VALUE",7},{"OUT_RS2_VALUE",0}});
    s.add("no-high-clock-rewrite",{{"WB_VALUE",9}},{{"OUT_RS1_VALUE",7}});
    s.add("x2-setup",{{"CLK",0},{"WB_RD",2},{"WB_VALUE",11}});s.edge("write-x2",{{"OUT_RS2_VALUE",11}});
    s.add("x0-setup",{{"CLK",0},{"WB_RD",0},{"IN_INSTRUCTION",0x00000013}});s.edge("x0",{{"OUT_RS1_VALUE",0}});
    for(auto ins:{0x00000063U,0x008000efU,0x00008067U}) s.add("control-flow-"+std::to_string(ins),{{"IN_INSTRUCTION",ins},{"REGISTER_WRITE",0}},{{"CONTROL_FLOW",1},{"TERMINAL",0}});
    s.add("halt",{{"IN_INSTRUCTION",0x00100073}},{{"TERMINAL",1},{"CONTROL_FLOW",0}});
    s.add("invalid-slot",{{"IN_VALID",0}},{{"TERMINAL",0}});
    s.add("fetch-fault",{{"IN_VALID",1},{"IN_INSTRUCTION",0x00000013},{"IN_FETCH_STATUS",2}},{{"TERMINAL",1}});
    s.add("reserved-status-ignored",{{"IN_FETCH_STATUS",0x80000000}},{{"TERMINAL",0}});return s.spec();
}
ComponentTestSpec executeSpec() {
    Scenario s(families::RV32IBaselineExecuteStage);
    s.add("addi",{{"IN_VALID",1},{"IN_CONTROL",controlWord(0x00508093)},{"IN_PC",0x100},{"IN_RS1_VALUE",7},{"IN_RS2_VALUE",0xdeadbeef},{"IN_IMMEDIATE",5}},
        {{"OUT_ALU_RESULT",12},{"OUT_PC_PLUS_4",0x104},{"OUT_NEXT_PC",0x104},{"OUT_STORE_DATA",0xdeadbeef},{"TERMINAL",0},{"CONTROL_FLOW",0}});
    s.add("beq-not-taken",{{"IN_CONTROL",controlWord(0x00208463)},{"IN_IMMEDIATE",8},{"IN_RS2_VALUE",8}},{{"OUT_NEXT_PC",0x104},{"CONTROL_FLOW",1},{"OUT_EXECUTION_STATUS",0}});
    s.add("beq-taken",{{"IN_RS2_VALUE",7}},{{"OUT_NEXT_PC",0x108}});
    s.add("taken-misaligned",{{"IN_IMMEDIATE",2}},{{"OUT_EXECUTION_STATUS",1},{"TERMINAL",1}});
    s.add("untaken-misaligned-target-is-harmless",{{"IN_RS2_VALUE",8}},{{"OUT_EXECUTION_STATUS",0},{"TERMINAL",0}});
    s.add("jal-shares-alu",{{"IN_CONTROL",controlWord(0x008000ef)},{"IN_IMMEDIATE",8}},{{"OUT_ALU_RESULT",0x108},{"OUT_NEXT_PC",0x108}});
    s.add("jalr-clears-bit-zero",{{"IN_CONTROL",controlWord(0x000080e7)},{"IN_RS1_VALUE",0x109},{"IN_IMMEDIATE",0}},{{"OUT_ALU_RESULT",0x109},{"OUT_NEXT_PC",0x108}});
    s.add("signed-branch",{{"IN_CONTROL",controlWord(0x0020c463)},{"IN_RS1_VALUE",0xffffffff},{"IN_RS2_VALUE",1},{"IN_IMMEDIATE",8}},{{"OUT_NEXT_PC",0x108}});
    s.add("unsigned-branch",{{"IN_CONTROL",controlWord(0x0020e463)}},{{"OUT_NEXT_PC",0x104}});
    s.add("invalid-slot",{{"IN_VALID",0},{"IN_FETCH_STATUS",3}},{{"TERMINAL",0},{"CONTROL_FLOW",0}});return s.spec();
}
ComponentTestSpec memorySpec() {
    Scenario s(families::RV32IBaselineMemoryStage);
    s.add("lb",{{"IN_VALID",1},{"IN_CONTROL",controlWord(0x00008083)},{"IN_ALU_RESULT",0x101},{"MEMORY_ENABLE",1},{"DMEM_READ_DATA",0x00008000}},
        {{"OUT_MEMORY_DATA",0xffffff80},{"DMEM_WORD_ADDR",0x40},{"DMEM_READ_EN",1},{"DMEM_WE",0},{"TERMINAL",0}});
    s.add("sb",{{"IN_CONTROL",controlWord(0x00208023)},{"IN_STORE_DATA",0xa5}},{{"DMEM_WRITE_MASK",2},{"DMEM_WRITE_DATA",0xa500},{"DMEM_WE",1}});
    s.add("endpoint-rejects-write",{{"DMEM_FAULT",1}},{{"DMEM_WE",1},{"OUT_MEMORY_STATUS",2},{"TERMINAL",1}});
    s.add("misaligned-sw",{{"DMEM_FAULT",0},{"IN_CONTROL",controlWord(0x0020a023)}},{{"DMEM_WE",0},{"OUT_MEMORY_STATUS",1},{"TERMINAL",1}});
    s.add("aligned-sw",{{"IN_ALU_RESULT",0x100}},{{"DMEM_WE",1},{"OUT_MEMORY_STATUS",0},{"TERMINAL",0}});
    s.add("older-terminal-blocks-store",{{"MEMORY_ENABLE",0}},{{"DMEM_WE",0},{"DMEM_READ_EN",0}});
    s.add("fetch-fault-blocks-memory",{{"MEMORY_ENABLE",1},{"IN_FETCH_STATUS",2}},{{"DMEM_WE",0},{"DMEM_READ_EN",0},{"TERMINAL",1}});
    s.add("bubble-has-no-effects",{{"IN_VALID",0}},{{"DMEM_WE",0},{"DMEM_READ_EN",0},{"TERMINAL",0}});return s.spec();
}
ComponentTestSpec dependencySpec() {
    Scenario s(families::RV32IBaselineDependencyUnit);
    s.add("two-sources",{{"D_VALID",1},{"D_CONTROL",(1<<7)|(1<<8)},{"D_REGISTER_ADDRESSES",1|(2<<5)}});
    for(const std::string stage:{"E","M","W"}) {
        const std::string wait=stage=="E"?"WAIT_EX":stage=="M"?"WAIT_MEM":"WAIT_WB";
        for(unsigned rd=0;rd<32;++rd) s.add(stage+"-destination-"+std::to_string(rd),{{stage+"_VALID",1},{stage+"_CONTROL",2},{stage+"_REGISTER_ADDRESSES",rd<<10}},{{"DATA_HAZARD",rd==1||rd==2},{wait,rd==1||rd==2}});
        s.add(stage+"-disabled-producer",{{stage+"_REGISTER_ADDRESSES",1<<10},{stage+"_CONTROL",0}},{{"DATA_HAZARD",0}});
        s.add(stage+"-bubble",{{stage+"_CONTROL",2},{stage+"_VALID",0}},{{"DATA_HAZARD",0}});
    }
    s.add("unused-encoded-source",{{"D_CONTROL",0},{"E_VALID",1}},{{"DATA_HAZARD",0}});
    s.add("invalid-decode",{{"D_CONTROL",1<<7},{"D_VALID",0}},{{"DATA_HAZARD",0}});return s.spec();
}
ComponentTestSpec controlSpec() {
    Scenario s(families::RV32IBaselineControl);
    s.add("normal-flow",{{"ENABLE",1},{"RST",0}},{{"FETCH_VALID",1},{"PC_WRITE",1},{"IF_ID_WRITE",1},{"ID_EX_FLUSH",0},{"MEMORY_ENABLE",1}});
    s.add("raw-wait",{{"DATA_HAZARD",1}},{{"FETCH_VALID",0},{"IF_ID_WRITE",0},{"ID_EX_WRITE",1},{"ID_EX_FLUSH",1},{"EX_MEM_FLUSH",0}});
    s.add("branch-waits-for-operand",{{"D_CONTROL_FLOW",1}},{{"IF_ID_WRITE",0},{"IF_ID_FLUSH",1},{"PC_REDIRECT",0}});
    s.add("branch-enters-execute",{{"DATA_HAZARD",0}},{{"IF_ID_WRITE",1},{"IF_ID_FLUSH",1},{"ID_EX_FLUSH",0},{"FETCH_VALID",0}});
    s.add("branch-resolves",{{"D_CONTROL_FLOW",0},{"E_CONTROL_FLOW",1}},{{"PC_WRITE",1},{"PC_REDIRECT",1},{"FETCH_VALID",0},{"ID_EX_FLUSH",1}});
    s.add("older-memory-fault-wins",{{"M_TERMINAL",1}},{{"PC_REDIRECT",0},{"EX_MEM_FLUSH",1},{"MEM_WB_FLUSH",0}});
    s.add("oldest-terminal-blocks-store",{{"W_TERMINAL",1}},{{"MEMORY_ENABLE",0},{"MEM_WB_FLUSH",1},{"COMMIT_ENABLE",1}});
    s.add("disabled-holds",{{"ENABLE",0}},{{"ACTIVE",0},{"IF_ID_WRITE",0},{"ID_EX_WRITE",0},{"EX_MEM_WRITE",0},{"MEM_WB_WRITE",0},{"COMMIT_ENABLE",0}});
    s.add("reset",{{"ENABLE",1},{"RST",1}},{{"ACTIVE",0},{"MEMORY_ENABLE",0}});
    // Independent truth-table expectation for all 11 control inputs.
    const std::vector<std::string> names{"ENABLE","RST","HALTED","TRAPPED","DATA_HAZARD","D_CONTROL_FLOW","E_CONTROL_FLOW","D_TERMINAL","E_TERMINAL","M_TERMINAL","W_TERMINAL"};
    for(unsigned mask=0;mask<2048;++mask) {
        NamedValues in;for(unsigned i=0;i<names.size();++i) in[names[i]]=logicBit(mask&(1U<<i));
        const auto bit=[&](unsigned i){return bool(mask&(1U<<i));};
        const bool active=bit(0)&&!bit(1)&&!bit(2)&&!bit(3),older=bit(8)||bit(9)||bit(10),stall=bit(4)&&!bit(7)&&!older;
        const bool term=older||(bit(7)&&!bit(6)),branch=bit(5)||bit(6),fetch=active&&!term&&!stall&&!branch,redirect=active&&bit(6)&&!older;
        s.actions.push_back({"control-inputs-"+std::to_string(mask),CheckpointKind::Settled,in,
            vals({{"ACTIVE",active},{"FETCH_VALID",fetch},{"PC_WRITE",fetch||redirect},{"PC_REDIRECT",redirect},
                {"IF_ID_WRITE",active&&(!stall||bit(6))},{"IF_ID_FLUSH",term||branch},{"ID_EX_WRITE",active},{"ID_EX_FLUSH",stall||older||bit(6)},
                {"EX_MEM_WRITE",active},{"EX_MEM_FLUSH",bit(9)||bit(10)},{"MEM_WB_WRITE",active},{"MEM_WB_FLUSH",bit(10)},
                {"MEMORY_ENABLE",active&&!bit(10)},{"COMMIT_ENABLE",active},{"DATA_STALL",active&&stall},{"BRANCH_WAIT",active&&branch},{"PIPELINE_FLUSH",active&&(term||branch)}},s.f,true)});
    }
    return s.spec();
}
BuildProfile profile() {return rv32i::architectureStructuralProfile(builtinComponentCatalog(),"baseline-structural-stages");}
ComponentTestSpec coreSpec() {
    Scenario s(families::RV32IBaselinePipelineCore);
    s.add("release-reset",{{"RST",0},{"ENABLE",1},{"IMEM_RSP_READ_DATA",0x00100093},
        {"IMEM_REQ_READY",1},{"IMEM_RSP_VALID",1},{"DMEM_REQ_READY",1},{"DMEM_RSP_VALID",1}},{{"COMMIT_VALID",0}});
    for(unsigned edge=1;edge<=5;++edge) {
        s.add("clock-low-"+std::to_string(edge),{{"CLK",0}});
        s.edge("fill-"+std::to_string(edge),{{"IF_ID_VALID",1},{"ID_EX_VALID",edge>=2},{"EX_MEM_VALID",edge>=3},{"MEM_WB_VALID",edge>=4},
            {"COMMIT_VALID",edge>=4},{"DATA_STALL",0}});
    }
    s.add("pause",{{"CLK",0},{"ENABLE",0}},{{"DMEM_REQ_VALID",0},{"IMEM_REQ_VALID",0}});
    s.edge("paused-edge",{{"COMMIT_VALID",0},{"FETCH_PC",20},{"IF_ID_PC",16},{"ID_EX_PC",12},{"EX_MEM_PC",8},{"MEM_WB_PC",4}});
    s.add("resume-with-halt",{{"CLK",0},{"ENABLE",1},{"IMEM_RSP_READ_DATA",0x00100073}});
    for(unsigned edge=6;edge<=10;++edge) {
        s.add("clock-low-"+std::to_string(edge),{{"CLK",0}});
        s.edge("drain-"+std::to_string(edge),{{"HALTED",edge==10},{"TRAPPED",0}});
    }
    s.add("post-halt-low",{{"CLK",0}});s.edge("halted-holds",{{"COMMIT_VALID",0},{"DMEM_REQ_VALID",0}});
    s.add("reset-flushes",{{"RST",1}},{{"COMMIT_VALID",0},{"FETCH_PC",0},{"IF_ID_VALID",0},{"ID_EX_VALID",0},{"EX_MEM_VALID",0},{"MEM_WB_VALID",0}});
    return s.spec();
}
}
#define TEST(Name,Spec) RV32IBaseline##Name##Test::RV32IBaseline##Name##Test():ComponentScenarioTest(Spec(),"contract",profile()) {}
TEST(FetchStage,fetchSpec) TEST(DecodeStage,decodeSpec) TEST(ExecuteStage,executeSpec) TEST(MemoryStage,memorySpec) TEST(DependencyUnit,dependencySpec) TEST(Control,controlSpec)
#undef TEST

RV32IBaselinePipelineCoreTest::RV32IBaselinePipelineCoreTest():ComponentScenarioTest(coreSpec(),"contract",profile()) {}
bool RV32IBaselinePipelineCoreTest::run() {
    if(!ComponentScenarioTest::run()) return false;
    // Includes hazards, branches, jumps, signed lanes, traps and long programs.
    // Each run compares both fidelities with the instruction oracle and with
    // each other; physical writes are checked separately from retirement.
    for(size_t program=1;program<=RV32IProgramCaseCount;++program) {
        RV32IBaselinePipelineProgramTest run(program);
        if(!run.run()) return false;
    }
    return true;
}
