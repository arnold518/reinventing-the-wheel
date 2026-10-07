#include "tests/RV32IPredictionTests.hpp"
#include "modules/rv32i/RV32IPrediction.hpp"
#include "modules/rv32i/RV32IBuildProfiles.hpp"
#include <algorithm>

namespace {
using namespace circuit::test;
using Policy=rv32i::PredictionPolicy;
ScenarioAction row(std::string name, NamedValues inputs, NamedValues outputs) {
    return {std::move(name),CheckpointKind::Settled,std::move(inputs),std::move(outputs),true,{}};
}
ScenarioAction drive(NamedValues inputs) {return {"",CheckpointKind::Settled,std::move(inputs),{},false,{}};}
ComponentTestSpec spec(const circuit::ComponentFamily& family) {
    return {std::string(family.typeName())+"Test",std::string(family.id()),"RV32I_PREDICTION_ROOT",{}, {}};
}
ComponentTestSpec counterSpec() {
    auto s=spec(circuit::families::RV32IPredictionCounter);
    ActionScenario a{"all-states",{}, {}};
    const unsigned up[]{1,2,3,3},down[]{0,0,1,2};
    for (unsigned mode=0;mode<2;++mode) for (unsigned state=0;state<4;++state) for (unsigned taken=0;taken<2;++taken)
        a.actions.push_back(row("mode-"+std::to_string(mode)+"-state-"+std::to_string(state)+"-taken-"+std::to_string(taken),
            {{"CURRENT",logicBits(2,state)},{"TAKEN",logicBit(bool(taken))},{"ONE_BIT",logicBit(bool(mode))}},
            {{"NEXT",logicBits(2,mode?(taken?3:0):(taken?up[state]:down[state]))}}));
    s.scenarios.push_back(std::move(a));return s;
}
ComponentTestSpec predecodeSpec() {
    auto s=spec(circuit::families::RV32IBranchPredecode);
    ActionScenario a{"targets",{}, {}};
    struct Case {const char* name;uint32_t pc,instruction,target;bool branch,jump,back;};
    for (auto c : {Case{"forward-branch",0x100,0x00208463,0x108,true,false,false},
                   Case{"backward-branch",0x100,0xfe208ee3,0xfc,true,false,true},
                   Case{"branch-to-fallthrough",0x100,0x00000263,0x104,true,false,false},
                   Case{"jal",0x100,0x008000ef,0x108,false,true,false},
                   Case{"jalr-needs-register",0x100,0x00008067,0x100,false,false,false},
                   Case{"reserved-branch",0x100,0x00002063,0x100,false,false,false},
                   Case{"ordinary",0x100,0x00108093,0x100,false,false,false},
                   Case{"wraparound",0xfffffffc,0x008000ef,4,false,true,false}})
        a.actions.push_back(row(c.name,{{"PC",logicBits(32,c.pc)},{"INSTRUCTION",logicBits(32,c.instruction)}},
            {{"CONDITIONAL",logicBit(c.branch)},{"DIRECT_JUMP",logicBit(c.jump)},{"BACKWARD",logicBit(c.back)},
             {"TARGET",logicBits(32,c.target)},{"PC_PLUS_4",logicBits(32,c.pc+4)}}));
    s.scenarios.push_back(std::move(a));return s;
}
ComponentTestSpec predictorSpec() {
    auto s=spec(circuit::families::RV32IBranchPredictor);
    s.parameters=rv32i::PredictionConfig{Policy::TwoBit,4}.parameters();
    // Put the table-based policy first so its preview shows actual state.
    for (auto policy : {Policy::TwoBit,Policy::OneBit,Policy::NotTaken,Policy::Taken,Policy::BackwardTaken}) {
      for(size_t entries:{4u,1u,16u}) {
        ActionScenario a{std::string(rv32i::predictionPolicyName(policy))+"-"+std::to_string(entries),rv32i::PredictionConfig{policy,entries}.parameters(),{}};
        const bool dynamic=policy==Policy::OneBit||policy==Policy::TwoBit;
        unsigned state=0;
        const auto outputs=[&](bool backward=false,bool branch=true,bool jump=false,uint32_t target=100,uint32_t next=4) {
            const bool direction=policy==Policy::Taken || (policy==Policy::BackwardTaken&&backward) || (dynamic&&state>=2);
            const bool taken=jump||(branch&&direction);
            return NamedValues{{"NEXT_PC",logicBits(32,taken?target:next)},{"TAKEN",logicBit(taken)},{"COUNTER",logicBits(2,state)}};
        };
        a.actions.push_back(row("reset",{{"CLK",logicBit(false)},{"RST",logicBit(true)},{"PC",logicBits(32,0)},
            {"CONDITIONAL",logicBit(true)},{"DIRECT_JUMP",logicBit(false)},{"BACKWARD",logicBit(false)},
            {"TARGET",logicBits(32,100)},{"PC_PLUS_4",logicBits(32,4)},{"UPDATE",logicBit(false)},
            {"UPDATE_PC",logicBits(32,0)},{"UPDATE_TAKEN",logicBit(false)}},outputs()));
        unsigned index=0;
        for (bool taken : {true,true,true,true,false,false,false,false,false,true,false,true,true}) {
            a.actions.push_back(drive({{"CLK",logicBit(false)},{"RST",logicBit(false)},
                {"UPDATE",logicBit(true)},{"UPDATE_TAKEN",logicBit(taken)}}));
            if (dynamic) state=policy==Policy::OneBit ? (taken?3:0) : unsigned(std::clamp(int(state)+(taken?1:-1),0,3));
            a.actions.push_back(row("train-"+std::to_string(index++),{{"CLK",logicBit(true)}},outputs()));
        }
        a.actions.push_back(row("backward-direction",{{"CLK",logicBit(false)},{"UPDATE",logicBit(false)},
            {"BACKWARD",logicBit(true)}},outputs(true)));
        a.actions.push_back(row("disabled-update",{{"CLK",logicBit(true)},{"UPDATE_TAKEN",logicBit(true)}},outputs(true)));
        const auto trained=state;
        if(entries>1) {
            state=0;
            a.actions.push_back(row("independent-row",{{"CLK",logicBit(false)},{"PC",logicBits(32,4)}},outputs(true)));
        }
        state=trained;
        a.actions.push_back(row("index-alias",{{"CLK",logicBit(false)},{"PC",logicBits(32,entries*4)}},outputs(true)));
        a.actions.push_back(row("jal-always-target",{{"CONDITIONAL",logicBit(false)},{"DIRECT_JUMP",logicBit(true)}},outputs(true,false,true)));
        a.actions.push_back(row("ordinary-fallthrough",{{"DIRECT_JUMP",logicBit(false)}},outputs(true,false,false)));
        state=0;
        a.actions.push_back(row("reset-clears-training",{{"RST",logicBit(true)},{"CONDITIONAL",logicBit(true)}},outputs(true)));
        s.scenarios.push_back(std::move(a));
      }
    }
    return s;
}
ComponentTestSpec recoverySpec() {
    auto s=spec(circuit::families::RV32IBranchRecovery);
    ActionScenario a{"priorities",{}, {}};
    for (unsigned flags=0;flags<256;++flags) {
        const bool valid=flags&1, terminal=flags&2, advance=flags&4, kill=flags&8,
                   branch=flags&16, reset=flags&32, differs=flags&64, taken=flags&128;
        const bool eligible=valid&&!terminal&&!reset, resolve=eligible&&advance&&!kill;
        a.actions.push_back(row("case-"+std::to_string(flags),{{"VALID",logicBit(valid)},{"PRETERMINAL",logicBit(terminal)},
            {"ADVANCE",logicBit(advance)},{"KILL",logicBit(kill)},{"IS_BRANCH",logicBit(branch)},{"RST",logicBit(reset)},
            {"PREDICTED_PC",logicBits(32,0x100)},{"TAKEN",logicBit(taken)},
            {"FALLTHROUGH_PC",logicBits(32,!taken?(differs?0x80000100u:0x100):0x200)},
            {"TARGET_PC",logicBits(32,taken?(differs?0x80000100u:0x100):0x200)}},
            {{"MISPREDICT",logicBit(eligible&&differs)},{"RESOLVE",logicBit(resolve)},{"UPDATE",logicBit(resolve&&branch)},
             {"RECOVERY",logicBit(resolve&&differs)},{"BRANCH_MISS",logicBit(resolve&&branch&&differs)}}));
    }
    for(bool taken:{false,true}) a.actions.push_back(row(taken?"same-address-taken":"same-address-fallthrough",
        {{"VALID",logicBit(true)},{"PRETERMINAL",logicBit(false)},{"ADVANCE",logicBit(true)},
         {"KILL",logicBit(false)},{"IS_BRANCH",logicBit(true)},{"RST",logicBit(false)},
         {"TAKEN",logicBit(taken)},{"PREDICTED_PC",logicBits(32,4)},
         {"FALLTHROUGH_PC",logicBits(32,4)},{"TARGET_PC",logicBits(32,4)}},
        {{"MISPREDICT",logicBit(false)},{"RECOVERY",logicBit(false)},{"UPDATE",logicBit(true)}}));
    s.scenarios.push_back(std::move(a));return s;
}
ComponentTestSpec pathSpec() {
    auto s=spec(circuit::families::RV32IPredictionPath);
    s.parameters=rv32i::PredictionConfig{Policy::Taken,4}.parameters();
    ActionScenario a{"metadata-and-recovery",{}, {}};
    NamedValues initial;
    for (auto p : {"CLK","RST","IF_ID_WRITE","ID_EX_WRITE","EX_VALID","EX_PRETERMINAL","EX_ADVANCE","EX_KILL",
                   "EX_IS_BRANCH","EX_TAKEN","EX_CONTROL_TAKEN","REDIRECT_APPLY"}) initial[p]=logicBit(std::string(p)=="RST");
    for (auto p : {"FETCH_PC","EX_PC","EX_FALLTHROUGH_PC","EX_TARGET_PC"}) initial[p]=logicBits(32,0);
    initial["FETCH_INSTRUCTION"]=logicBits(32,0x00208463);
    a.actions.push_back(row("reset",initial,{{"PREDICTED_NEXT_PC",logicBits(32,8)},{"FETCH_NEXT_PC",logicBits(32,8)},
        {"EX_PREDICTED_NEXT_PC",logicBits(32,0)},{"MISPREDICT",logicBit(false)},{"UPDATE",logicBit(false)}}));
    a.actions.push_back(drive({{"RST",logicBit(false)},{"IF_ID_WRITE",logicBit(true)},{"ID_EX_WRITE",logicBit(true)}}));
    a.actions.push_back(row("capture-if-id",{{"CLK",logicBit(true)}},{{"EX_PREDICTED_NEXT_PC",logicBits(32,0)}}));
    a.actions.push_back(drive({{"CLK",logicBit(false)},{"FETCH_PC",logicBits(32,4)},{"FETCH_INSTRUCTION",logicBits(32,0xfe208ee3)}}));
    a.actions.push_back(row("capture-id-ex",{{"CLK",logicBit(true)}},{{"EX_PREDICTED_NEXT_PC",logicBits(32,8)},
        {"PREDICTED_NEXT_PC",logicBits(32,0)}}));
    a.actions.push_back(row("wait-holds-update",{{"CLK",logicBit(false)},{"IF_ID_WRITE",logicBit(false)},{"ID_EX_WRITE",logicBit(false)},
        {"EX_VALID",logicBit(true)},{"EX_IS_BRANCH",logicBit(true)},{"EX_FALLTHROUGH_PC",logicBits(32,4)}},
        {{"MISPREDICT",logicBit(true)},{"UPDATE",logicBit(false)},{"RESOLVE",logicBit(false)}}));
    a.actions.push_back(row("held-clock",{{"CLK",logicBit(true)}},{{"EX_PREDICTED_NEXT_PC",logicBits(32,8)}}));
    a.actions.push_back(row("apply-correction",{{"CLK",logicBit(false)},{"EX_ADVANCE",logicBit(true)},
        {"REDIRECT_APPLY",logicBit(true)}},{{"FETCH_NEXT_PC",logicBits(32,4)},{"UPDATE",logicBit(true)}}));
    a.actions.push_back(row("older-fault-kills-training",{{"EX_KILL",logicBit(true)}},{{"UPDATE",logicBit(false)},{"RESOLVE",logicBit(false)}}));
    a.actions.push_back(row("bubble-does-not-resolve",{{"EX_VALID",logicBit(false)}},{{"MISPREDICT",logicBit(false)}}));
    s.scenarios.push_back(std::move(a));
    ActionScenario collision{"read-before-update",rv32i::PredictionConfig{Policy::TwoBit,4}.parameters(),{}};
    collision.actions.push_back(row("reset",initial,{{"PREDICTED_NEXT_PC",logicBits(32,4)}}));
    collision.actions.push_back(drive({{"RST",logicBit(false)},{"IF_ID_WRITE",logicBit(true)},{"ID_EX_WRITE",logicBit(true)},
        {"EX_VALID",logicBit(true)},{"EX_IS_BRANCH",logicBit(true)},{"EX_ADVANCE",logicBit(true)},{"EX_TAKEN",logicBit(true)},
        {"EX_FALLTHROUGH_PC",logicBits(32,8)}}));
    collision.actions.push_back(row("first-training",{{"CLK",logicBit(true)}},{{"PREDICTED_NEXT_PC",logicBits(32,4)}}));
    collision.actions.push_back(drive({{"CLK",logicBit(false)}}));
    collision.actions.push_back(row("prediction-switches-after-edge",{{"CLK",logicBit(true)}},
        {{"PREDICTED_NEXT_PC",logicBits(32,8)},{"EX_PREDICTED_NEXT_PC",logicBits(32,4)}}));
    collision.actions.push_back(drive({{"CLK",logicBit(false)},{"EX_ADVANCE",logicBit(false)}}));
    collision.actions.push_back(row("captured-old-lookup",{{"CLK",logicBit(true)}},{{"EX_PREDICTED_NEXT_PC",logicBits(32,4)}}));
    collision.actions.push_back(drive({{"CLK",logicBit(false)}}));
    collision.actions.push_back(row("new-lookup-follows-one-cycle-later",{{"CLK",logicBit(true)}},{{"EX_PREDICTED_NEXT_PC",logicBits(32,8)}}));
    s.scenarios.push_back(std::move(collision));return s;
}
}
#define DEFINE_TEST(Name, Spec) \
Name::Name():circuit::test::ComponentScenarioTest(Spec(),{},rv32i::withBehavioralMemoryParts(circuit::canonicalDefaultProfile(),"prediction-preview")) {}
DEFINE_TEST(RV32IPredictionCounterTest,counterSpec)
DEFINE_TEST(RV32IBranchPredecodeTest,predecodeSpec)
DEFINE_TEST(RV32IBranchPredictorTest,predictorSpec)
DEFINE_TEST(RV32IBranchRecoveryTest,recoverySpec)
DEFINE_TEST(RV32IPredictionPathTest,pathSpec)
#undef DEFINE_TEST
