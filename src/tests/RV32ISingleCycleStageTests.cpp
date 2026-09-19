#include "tests/RV32ISingleCycleStageTests.hpp"
#include "tests/RV32IBlockStandaloneTests.hpp"
#include "tests/RV32ISingleCycleTests.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/composite/ALU32.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <iostream>
#include <optional>

using namespace circuit;
using namespace circuit::test;
namespace {
struct Scenario : ActionScenario {
    Scenario():ActionScenario{"contract",{},{},{500'000,10'000'000}} {}
    void add(std::string name,NamedValues input,NamedValues output={}) {
        actions.push_back({std::move(name),CheckpointKind::Settled,std::move(input),std::move(output)});
    }
    void edge(std::string name,NamedValues output={}) {
        actions.push_back({std::move(name),CheckpointKind::AfterEdge,{{"CLK",logicBit(true)}},std::move(output)});
    }
};
ComponentTestSpec spec(const ComponentFamily& f,ActionScenario s) {
    return {std::string(f.typeName())+"Test",std::string(f.id()),std::string(f.typeName())+"_ROOT",{},{std::move(s)}};
}
ComponentTestSpec fetchSpec() {
    Scenario s;
    s.add("reset",{{"CLK",logicBit(false)},{"RST",logicBit(true)},{"ISSUE",logicBit(true)},
        {"TAKE",logicBit(false)},{"PC_WRITE",logicBit(false)},{"NEXT_PC",logicBits(32,16)},
        {"IMEM_REQ_READY",logicBit(false)},{"IMEM_RSP_VALID",logicBit(false)},
        {"IMEM_RSP_READ_DATA",logicBits(32,0)},{"IMEM_RSP_STATUS",logicBits(3,0)}},
        {{"PC",logicBits(32,0)},{"PC_PLUS_4",logicBits(32,4)},{"IMEM_REQ_VALID",logicBit(false)}});
    s.add("release",{{"RST",logicBit(false)}});
    s.edge("capture-old-pc",{{"IMEM_REQ_VALID",logicBit(true)},{"IMEM_REQ_ADDR",logicBits(32,0)}});
    s.add("change-uncommitted-next-pc",{{"CLK",logicBit(false)},{"NEXT_PC",logicBits(32,24)}});
    s.edge("hold-request",{{"PC",logicBits(32,0)},{"IMEM_REQ_ADDR",logicBits(32,0)}});
    s.add("accept-low",{{"CLK",logicBit(false)},{"IMEM_REQ_READY",logicBit(true)}});
    s.edge("accept",{{"DONE",logicBit(false)},{"IMEM_REQ_VALID",logicBit(false)}});
    s.add("response",{{"CLK",logicBit(false)},{"IMEM_RSP_VALID",logicBit(true)},
        {"IMEM_RSP_READ_DATA",logicBits(32,0x01000093)}},
        {{"DONE",logicBit(true)},{"INSTRUCTION",logicBits(32,0x01000093)},{"IMEM_RSP_READY",logicBit(false)}});
    s.edge("response-backpressure",{{"PC",logicBits(32,0)},{"DONE",logicBit(true)}});
    s.add("authorize-completion",{{"CLK",logicBit(false)},{"TAKE",logicBit(true)},{"PC_WRITE",logicBit(true)},{"NEXT_PC",logicBits(32,16)}},
        {{"PC",logicBits(32,0)},{"IMEM_RSP_READY",logicBit(true)}});
    s.edge("commit",{{"PC",logicBits(32,16)},{"PC_PLUS_4",logicBits(32,20)},{"DONE",logicBit(false)}});
    s.add("stop",{{"CLK",logicBit(false)},{"ISSUE",logicBit(false)},{"TAKE",logicBit(false)},
        {"PC_WRITE",logicBit(false)},{"IMEM_RSP_VALID",logicBit(false)}});
    s.edge("no-new-fetch",{{"IMEM_REQ_VALID",logicBit(false)},{"PC",logicBits(32,16)}});
    s.add("resume",{{"CLK",logicBit(false)},{"ISSUE",logicBit(true)}});
    s.edge("next-request",{{"IMEM_REQ_ADDR",logicBits(32,16)},{"IMEM_REQ_VALID",logicBit(true)}});
    s.add("reset-outstanding-request",{{"RST",logicBit(true)}},{{"PC",logicBits(32,0)},{"IMEM_REQ_VALID",logicBit(false)},{"IMEM_RSP_READY",logicBit(false)}});
    return spec(families::RV32ISingleCycleFetchStage,s);
}
ComponentTestSpec decodeSpec() {
    RV32IDecodeControlUnitTest base;
    Scenario s;
    s.add("reset",{{"CLK",logicBit(false)},{"RST",logicBit(true)},{"REGISTER_WRITE",logicBit(false)},
        {"WB_VALUE",logicBits(32,0)},{"INSTRUCTION",logicBits(32,0)}},
        {{"RS1_VALUE",logicBits(32,0)},{"RS2_VALUE",logicBits(32,0)}});
    s.add("release",{{"RST",logicBit(false)}});
    for(auto& scenario:base.getSpec().scenarios) for(auto action:scenario.actions) s.actions.push_back(std::move(action));
    s.add("write-setup-no-bypass",{{"INSTRUCTION",logicBits(32,0x00208093)},{"WB_VALUE",logicBits(32,7)},{"REGISTER_WRITE",logicBit(true)}},
        {{"RS1_VALUE",logicBits(32,0)},{"RD_ADDR",logicBits(5,1)}});
    s.edge("write-x1",{{"RS1_VALUE",logicBits(32,7)}});
    s.add("high-clock-does-not-write-again",{{"WB_VALUE",logicBits(32,9)}},{{"RS1_VALUE",logicBits(32,7)}});
    s.add("falling-edge-does-not-write",{{"CLK",logicBit(false)}},{{"RS1_VALUE",logicBits(32,7)}});
    s.edge("second-write",{{"RS1_VALUE",logicBits(32,9)}});
    s.add("x0-setup",{{"CLK",logicBit(false)},{"INSTRUCTION",logicBits(32,0x00000013)},{"WB_VALUE",logicBits(32,0xffffffff)}});
    s.edge("x0-hardwired",{{"RS1_VALUE",logicBits(32,0)}});
    s.add("reset-registers",{{"RST",logicBit(true)},{"INSTRUCTION",logicBits(32,0x00208093)}},{{"RS1_VALUE",logicBits(32,0)}});
    return spec(families::RV32ISingleCycleDecodeStage,s);
}
ComponentTestSpec executeSpec() {
    Scenario s;
    const NamedValues defaults{{"PC",logicBits(32,0x100)},{"RS1_VALUE",logicBits(32,0x80000000)},
        {"RS2_VALUE",logicBits(32,1)},{"IMM",logicBits(32,12)},{"ALU_A_SEL",logicBits(2,0)},
        {"ALU_B_SEL",logicBits(2,0)},{"ALU_OP",logicBits(5,0)},{"BRANCH_TYPE",logicBits(3,0)},{"JUMP_TYPE",logicBits(2,0)}};
    s.add("inputs",defaults);
    const uint32_t expected[]{0x80000001,0x7fffffff,0,0x80000001,0x80000001,0,0x40000000,0xc0000000,1,0,0x80000000,1,0};
    // ALU32 opcode values are contiguous through ZERO; check reserved codes too.
    for(unsigned op=0;op<32;++op) s.add("alu-op-"+std::to_string(op),{{"ALU_OP",logicBits(5,op)}},
        {{"ALU_RESULT",logicBits(32,op<13?expected[op]:0)}});
    s.add("address-and-jalr",{{"ALU_OP",logicBits(5,ALU32Op::ADD)}, {"RS1_VALUE",logicBits(32,0x101)},
        {"ALU_B_SEL",logicBits(2,1)},{"IMM",logicBits(32,4)},{"JUMP_TYPE",logicBits(2,2)}},
        {{"ALU_RESULT",logicBits(32,0x105)},{"CONTROL_TARGET",logicBits(32,0x104)}});
    s.add("jal-shares-alu",{{"ALU_A_SEL",logicBits(2,1)},{"JUMP_TYPE",logicBits(2,1)}},
        {{"ALU_RESULT",logicBits(32,0x104)},{"CONTROL_TARGET",logicBits(32,0x104)}});
    s.add("signed-unsigned-branch",{{"RS1_VALUE",logicBits(32,0xffffffff)},{"RS2_VALUE",logicBits(32,1)},
        {"ALU_OP",logicBits(5,ALU32Op::SUB)},{"ALU_A_SEL",logicBits(2,0)},{"ALU_B_SEL",logicBits(2,0)},
        {"JUMP_TYPE",logicBits(2,0)},{"IMM",logicBits(32,0xfffffffc)}});
    const bool taken[]{false,false,true,true,false,false,true,false};
    for(unsigned branch=0;branch<8;++branch) s.add("branch-"+std::to_string(branch),{{"BRANCH_TYPE",logicBits(3,branch)}},
        {{"ALU_RESULT",logicBits(32,0xfffffffe)},{"CONTROL_TARGET",logicBits(32,0xfc)},{"BRANCH_TAKEN",logicBit(taken[branch])}});
    s.add("equal-branch",{{"RS1_VALUE",logicBits(32,1)},{"BRANCH_TYPE",logicBits(3,1)}},{{"BRANCH_TAKEN",logicBit(true)}});
    s.add("wrap-target",{{"PC",logicBits(32,0xfffffffc)},{"IMM",logicBits(32,8)}},{{"CONTROL_TARGET",logicBits(32,4)}});
    return spec(families::RV32ISingleCycleExecuteStage,s);
}
ComponentTestSpec memorySpec() {
    Scenario s;
    s.add("reset",{{"CLK",logicBit(false)},{"RST",logicBit(true)},{"ISSUE",logicBit(false)},{"TAKE",logicBit(false)},
        {"MEM_WRITE",logicBit(false)},{"SIGN_EXTEND",logicBit(false)},{"ADDR",logicBits(32,0)},
        {"WRITE_DATA",logicBits(32,0)},{"SIZE",logicBits(2,2)},{"DMEM_REQ_READY",logicBit(false)},
        {"DMEM_RSP_VALID",logicBit(false)},{"DMEM_RSP_READ_DATA",logicBits(32,0)},{"DMEM_RSP_STATUS",logicBits(3,0)}});
    s.add("release",{{"RST",logicBit(false)}});
    struct Row {unsigned offset,size;bool write,sign;uint32_t response,result;unsigned mask,status;};
    const Row rows[]{ {1,0,false,true,0x8000,0xffffff80,2,0},{3,0,false,false,0x80000000,0x80,8,0},
        {2,1,false,true,0x80010000,0xffff8001,12,0},{0,1,false,false,0xfffe,0xfffe,3,0},
        {0,2,false,false,0x12345678,0x12345678,15,0},{2,0,true,false,0,0,4,0},
        {2,1,true,false,0,0,12,0},{0,2,true,false,0,0,15,0},{2,2,true,false,0,0,15,3},
        {0,3,false,false,0,0,0,3},{0,2,false,false,0,0,15,2}};
    size_t n=0;
    for(auto r:rows) {
        const auto label=std::to_string(n++);const bool bad=r.size==3||(r.size==1&&(r.offset&1))||(r.size==2&&r.offset);
        s.add("setup-"+label,{{"CLK",logicBit(false)},{"ISSUE",logicBit(true)},{"TAKE",logicBit(false)},
            {"ADDR",logicBits(32,0x100+r.offset)},{"SIZE",logicBits(2,r.size)},{"MEM_WRITE",logicBit(r.write)},
            {"SIGN_EXTEND",logicBit(r.sign)},{"WRITE_DATA",logicBits(32,0xdeadbeef)},
            {"DMEM_REQ_READY",logicBit(false)},{"DMEM_RSP_VALID",logicBit(false)},{"DMEM_RSP_STATUS",logicBits(3,0)}});
        s.edge("capture-"+label,{{"DMEM_REQ_VALID",logicBit(true)},{"DMEM_REQ_ADDR",logicBits(32,0x100+bad)},
            {"DMEM_REQ_OP",logicBits(3,r.write)},{"DMEM_REQ_WRITE_MASK",logicBits(4,r.mask)},
            {"DMEM_REQ_WRITE_DATA",logicBits(32,uint32_t(0xdeadbeefU<<(r.offset*8)))},{"MISALIGNED",logicBit(bad)}});
        s.add("backpressure-low-"+label,{{"CLK",logicBit(false)},{"WRITE_DATA",logicBits(32,0)}});
        s.edge("payload-held-"+label,{{"DMEM_REQ_WRITE_DATA",logicBits(32,uint32_t(0xdeadbeefU<<(r.offset*8)))},{"DONE",logicBit(false)}});
        s.add("accept-low-"+label,{{"CLK",logicBit(false)},{"DMEM_REQ_READY",logicBit(true)}});
        s.edge("accept-"+label,{{"DONE",logicBit(false)},{"DMEM_REQ_VALID",logicBit(false)}});
        s.add("respond-"+label,{{"CLK",logicBit(false)},{"DMEM_RSP_VALID",logicBit(true)},
            {"DMEM_RSP_STATUS",logicBits(3,r.status)},{"DMEM_RSP_READ_DATA",logicBits(32,r.response)}},
            {{"DONE",logicBit(true)},{"FAULT",logicBit(r.status!=0)},{"READ_DATA",logicBits(32,r.result)},{"DMEM_RSP_READY",logicBit(false)}});
        s.edge("response-held-"+label,{{"DONE",logicBit(true)}});
        s.add("take-low-"+label,{{"CLK",logicBit(false)},{"TAKE",logicBit(true)}},{{"DMEM_RSP_READY",logicBit(true)}});
        s.edge("consume-"+label,{{"DONE",logicBit(false)},{"DMEM_REQ_VALID",logicBit(false)}});
    }
    return spec(families::RV32ISingleCycleMemoryStage,s);
}
ComponentTestSpec writebackSpec() {
    Scenario s;
    s.add("inputs",{{"ALU_RESULT",logicBits(32,0xdeadbeef)},{"READ_DATA",logicBits(32,0x80000000)},
        {"PC_PLUS_4",logicBits(32,0x104)},{"WRITEBACK_SEL",logicBits(2,0)}});
    const uint32_t values[]{0,0xdeadbeef,0x80000000,0x104};
    for(unsigned i=0;i<4;++i) s.add("select-"+std::to_string(i),{{"WRITEBACK_SEL",logicBits(2,i)}},{{"WB_VALUE",logicBits(32,values[i])}});
    return spec(families::RV32ISingleCycleWritebackStage,s);
}
ComponentTestSpec nextPCSpec() {
    Scenario s;
    for(unsigned jump=0;jump<4;++jump) for(bool branch:{false,true}) for(unsigned target:{0x100u,0x102u,0xfffffffcu}) {
        const bool taken=jump||branch;
        s.add("select-"+std::to_string(jump)+"-"+std::to_string(branch)+"-"+std::to_string(target),
            {{"PC_PLUS_4",logicBits(32,4)},{"CONTROL_TARGET",logicBits(32,target)},
             {"BRANCH_TAKEN",logicBit(branch)},{"JUMP_TYPE",logicBits(2,jump)}},
            {{"NEXT_PC",logicBits(32,taken?target:4)},{"TARGET_MISALIGNED",logicBit(taken&&(target&3))}});
    }
    return spec(families::RV32ISingleCycleNextPCStage,s);
}
class NoReadingBackRun : public RV32ISystemProfileRun {
protected:
    rv32i::RV32IState snapshotComponentState() const override {
        auto state=RV32ISystemProfileRun::snapshotComponentState();
        if(previous_ && state.instruction_count==previous_->instruction_count
            && (state.pc!=previous_->pc || state.x!=previous_->x))
            throw std::runtime_error("Architectural state changed before instruction completion");
        previous_=state;
        return state;
    }
    RV32ISystemProgramCase getCase() const override {
        RV32ISystemProgramCase c;
        c.name="RV32ISingleCycleNoReadingBack";
        c.program=rv32i::RV32IProgram::fromWords({
            0x10000093, // addi x1,x0,256
            0x0000a083, // lw x1,0(x1): old x1 supplies address
            0x00208093, // addi x1,x1,2
            0x01800293, // addi x5,x0,24
            0x000282e7, // jalr x5,0(x5): target=24, link=20
            0x00000073, // ecall: must be skipped
            0x001080b3, // add x1,x1,x1: both old operands
            0x0010a023, // sw x1,0(x1): source supplies data and address
            0x00100073});
        c.initial_data={{0x100,{0,2,0,0}}};c.max_instructions=12;
        c.max_cycles_per_instruction=64;c.cycle_time_step=4000;
        c.expected_result.defined=true;c.expected_result.pc=32;
        c.expected_result.instruction_count=8;c.expected_result.halted=true;
        c.expected_result.registers={{1,1028},{5,20}};
        c.expected_result.bus_writes={{0x404,4},{0x405,4},{0x406,0},{0x407,0}};
        return c;
    }
private:
    mutable std::optional<rv32i::RV32IState> previous_;
};
}
#define DEFINE_TEST(Name,Spec) RV32ISingleCycle##Name##StageTest::RV32ISingleCycle##Name##StageTest() \
    : ComponentScenarioTest(Spec(),"contract",rv32i::withBehavioralMemoryParts(canonicalDefaultProfile())) {}
DEFINE_TEST(Fetch,fetchSpec)
DEFINE_TEST(Decode,decodeSpec)
DEFINE_TEST(Execute,executeSpec)
DEFINE_TEST(Memory,memorySpec)
DEFINE_TEST(Writeback,writebackSpec)
DEFINE_TEST(NextPC,nextPCSpec)
#undef DEFINE_TEST

void RV32ISingleCycleStageIntegrationTest::verifyResults() {
    // Each replacement must work in the real core, not just in isolation.
    const std::vector<std::string> stages{"FETCH","DECODE","EXECUTE","MEMORY","WRITEBACK","NEXT_PC"};
    for(const auto& mode:{"structural","FETCH","DECODE","EXECUTE","MEMORY","WRITEBACK","NEXT_PC","all-stages","whole-core","register-file"}) {
        NoReadingBackRun run;
        std::vector<ProfileRule> rules{preferFidelity(Fidelity::Structural,ProfileSelector::exactPath("RV32I_PROGRAM_ROOT.CORE"))};
        for(const auto& stage:stages) rules.push_back(preferFidelity(
            std::string(mode)==stage||std::string(mode)=="all-stages"?Fidelity::Behavioral:Fidelity::Structural,
            ProfileSelector::exactPath("RV32I_PROGRAM_ROOT.CORE."+stage)));
        if(std::string(mode)=="whole-core") rules.push_back(preferFidelity(Fidelity::Behavioral,ProfileSelector::exactPath("RV32I_PROGRAM_ROOT.CORE")));
        if(std::string(mode)=="register-file") {
            rules.push_back(preferFidelity(Fidelity::Structural,ProfileSelector::exactPath("RV32I_PROGRAM_ROOT.CORE.DECODE.REGISTER_FILE")));
            rules.push_back(preferFidelity(Fidelity::Behavioral,ProfileSelector::exactPath("RV32I_PROGRAM_ROOT.CORE.DECODE.REGISTER_FILE.STORAGE")));
        }
        run.setBuildProfile(withProfileOverrides(run.getBuildProfile(),rules));
        if(!run.run()) throw std::runtime_error("No-reading-back integration failed: "+std::string(mode));
        if(std::string(mode)=="structural") {
            std::shared_ptr<Component> core;
            for(auto c:run.getRoot()->getChildren()) if(c->getName()=="CORE") core=c;
            if(!core||core->getChildren().size()!=7) throw std::runtime_error("Core must contain six stages plus status");
            auto typed=std::dynamic_pointer_cast<RV32ISingleCycleCore>(core);
            size_t alus=0,adders=0;
            for(auto child:typed->executeStage()->getChildren()) {
                alus+=std::string(child->getTypeName())=="ALU32";
                adders+=std::string(child->getTypeName())=="Adder32";
                if(child->getName()=="RS1_IMM_ADD") throw std::runtime_error("Duplicate JALR adder");
            }
            if(alus!=1||adders!=1) throw std::runtime_error("Execute must share one ALU plus one concurrent branch adder");
        }
        std::cout<<"no-reading-back and memory effects: "<<mode<<" passed\n";
    }
}
