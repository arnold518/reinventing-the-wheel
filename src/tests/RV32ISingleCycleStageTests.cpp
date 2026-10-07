#include "tests/RV32ISingleCycleStageTests.hpp"
#include "tests/RV32IBlockStandaloneTests.hpp"
#include "tests/RV32ISingleCycleTests.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/composite/ALU32.hpp"
#include "components/selection/StandardProfiles.hpp"
#include <iostream>
#include <optional>
#include <set>

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
    s.add("reset",{{"CLK",logicBit(false)},{"RST",logicBit(true)},{"ISSUE",logicBit(false)},
        {"PC_WRITE",logicBit(false)},{"NEXT_PC",logicBits(32,16)},
        {"IMEM_READ_DATA",logicBits(32,0)},{"IMEM_FAULT",logicBit(false)}},
        {{"PC",logicBits(32,0)},{"PC_PLUS_4",logicBits(32,4)},{"IMEM_READ_EN",logicBit(false)}});
    s.add("read-without-clock",{{"RST",logicBit(false)},{"ISSUE",logicBit(true)},
        {"IMEM_READ_DATA",logicBits(32,0x01000093)}},
        {{"DONE",logicBit(true)},{"INSTRUCTION",logicBits(32,0x01000093)},{"IMEM_WORD_ADDR",logicBits(32,0)}});
    s.add("next-pc-not-yet-committed",{{"PC_WRITE",logicBit(true)}},{{"PC",logicBits(32,0)}});
    s.edge("commit",{{"PC",logicBits(32,16)},{"PC_PLUS_4",logicBits(32,20)},{"IMEM_WORD_ADDR",logicBits(32,4)}});
    s.add("high-clock-holds-pc",{{"NEXT_PC",logicBits(32,24)}},{{"PC",logicBits(32,16)}});
    s.add("falling-edge",{{"CLK",logicBit(false)}},{{"PC",logicBits(32,16)}});
    s.edge("second-commit",{{"PC",logicBits(32,24)}});
    s.add("disabled",{{"CLK",logicBit(false)},{"ISSUE",logicBit(false)},{"PC_WRITE",logicBit(false)}},
        {{"IMEM_READ_EN",logicBit(false)}});
    s.edge("hold",{{"PC",logicBits(32,24)}});
    s.add("fault-propagates",{{"IMEM_FAULT",logicBit(true)}},{{"FAULT",logicBit(true)}});
    s.add("reset-pc",{{"RST",logicBit(true)}},{{"PC",logicBits(32,0)}});
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
    struct Row {unsigned offset,size;bool write,sign;uint32_t response,result;unsigned mask,status;};
    const Row rows[]{ {1,0,false,true,0x8000,0xffffff80,2,0},{3,0,false,false,0x80000000,0x80,8,0},
        {2,1,false,true,0x80010000,0xffff8001,12,0},{0,1,false,false,0xfffe,0xfffe,3,0},
        {0,2,false,false,0x12345678,0x12345678,15,0},{2,0,true,false,0,0,4,0},
        {2,1,true,false,0,0,12,0},{0,2,true,false,0,0,15,0},{2,2,true,false,0,0,15,3},
        {0,3,false,false,0,0,0,3},{0,2,false,false,0,0,15,2}};
    size_t n=0;
    for(auto r:rows) {
        const auto label=std::to_string(n++);
        const bool bad=r.size==3||(r.size==1&&(r.offset&1))||(r.size==2&&r.offset);
        s.add("direct-access-"+label,{{"ISSUE",logicBit(true)},{"COMMIT",logicBit(false)},
            {"ADDR",logicBits(32,0x100+r.offset)},{"SIZE",logicBits(2,r.size)},{"MEM_WRITE",logicBit(r.write)},
            {"SIGN_EXTEND",logicBit(r.sign)},{"WRITE_DATA",logicBits(32,0xdeadbeef)},
            {"DMEM_FAULT",logicBit(r.status!=0)},{"DMEM_READ_DATA",logicBits(32,r.response)}},
            {{"DONE",logicBit(true)},{"FAULT",logicBit(r.status!=0)},{"READ_DATA",logicBits(32,r.result)},
             {"DMEM_WORD_ADDR",logicBits(32,0x40)},{"DMEM_WRITE_MASK",logicBits(4,r.mask)},
             {"DMEM_WRITE_DATA",logicBits(32,uint32_t(0xdeadbeefU<<(r.offset*8)))},
             {"MISALIGNED",logicBit(bad)},{"DMEM_WE",logicBit(false)}});
        s.add("authorize-"+label,{{"COMMIT",logicBit(r.status==0)}},
            {{"DMEM_WE",logicBit(r.write&&!bad&&r.status==0)}});
        s.add("payload-is-not-buffered-"+label,{{"WRITE_DATA",logicBits(32,0)}},
            {{"DMEM_WRITE_DATA",logicBits(32,0)}});
        s.add("disabled-"+label,{{"ISSUE",logicBit(false)}},
            {{"DMEM_WE",logicBit(false)},{"DMEM_READ_EN",logicBit(false)}});
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
        c.max_cycles_per_instruction=1;c.cycle_time_step=4000;
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
            rules.push_back(preferFidelity(Fidelity::Behavioral,ProfileSelector::contract("memory.register.32")));
        }
        run.setBuildProfile(withProfileOverrides(run.getBuildProfile(),rules));
        if(!run.run()) throw std::runtime_error("No-reading-back integration failed: "+std::string(mode));
        if(std::string(mode)=="structural") {
            std::shared_ptr<Component> core;
            for(auto c:run.getRoot()->getChildren()) if(c->getName()=="CORE") core=c;
            std::set<std::string> children;
            if(core) for(const auto& child:core->getChildren()) children.insert(child->getName());
            const std::set<std::string> expected{"FETCH","DECODE","EXECUTE","MEMORY","WRITEBACK","NEXT_PC",
                "EXECUTION_STATUS","INSTRUCTION_PORT","DATA_PORT","INSTRUCTION_NO_WRITE",
                "INSTRUCTION_WRITE_ZERO","INSTRUCTION_MASK_ZERO","STORE_AUTHORIZED"};
            if(children!=expected) throw std::runtime_error("Core must contain six stages, status, two combinational ports and four constants");
            auto typed=std::dynamic_pointer_cast<RV32ISingleCycleCore>(core);
            size_t alus=0,adders=0;
            for(auto child:typed->executeStage()->getChildren()) {
                alus+=std::string(child->getTypeName())=="ALU32Core";
                adders+=std::string(child->getTypeName())=="Adder32";
                if(child->getName()=="RS1_IMM_ADD") throw std::runtime_error("Duplicate JALR adder");
            }
            if(alus!=1||adders!=1) throw std::runtime_error("Execute must share one ALU plus one concurrent branch adder");
        }
        std::cout<<"no-reading-back and memory effects: "<<mode<<" passed\n";
    }
}
