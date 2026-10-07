#include "modules/rv32i/RV32IPipelineStages.hpp"

#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/IncrementBy4.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/Register.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPipelineEncoding.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "RV32IPipelineStagesInternal.hpp"

using namespace rv32i::pipeline_stages;

RV32IExecuteStage::RV32IExecuteStage(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IExecuteStage.pinInitializer()) {}

namespace {
// Wiring-only groups: selectable children retain their own fidelity, and the
// enclosing Execute stage supplies the behavioral equivalent of this assembly.
class ForwardedOperands : public IOComponent {
public:
    explicit ForwardedOperands(std::string name)
        : IOComponent(std::move(name),[](IOComponent* s) {
            circuit::families::RV32IForwardingUnit.pinInitializer()(s);
            for(auto p:{"RS1_VALUE","RS2_VALUE","EX_MEM_VALUE","MEM_WB_VALUE"}) s->addPin<32>(p,PinType::INPUT);
            for(auto p:{"VALUE_A","VALUE_B"}) s->addPin<32>(p,PinType::OUTPUT);
        }) {}
    const char* getTypeName() const override {return "RV32IForwardedOperands";}
    void buildInternals(ComponentBuilder& b) override {
        circuit::memory_detail::StructuralWiring w(b);
        auto select=b.add(circuit::families::RV32IForwardingUnit,"SELECT");
        auto zero=b.addNewComponent<ConstantValue<32>>("ZERO",0);
        for(auto& [name,pin]:select->getAllInputPins()) w.connect(getInputPinDynamic(name),pin);
        for(const std::string side:{"A","B"}) {
            auto mux=b.addNewComponent<Mux4to1_32bit>("OPERAND_"+side);
            w.connect(getInputPin<32>(side=="A"?"RS1_VALUE":"RS2_VALUE"),mux->getInputPin<32>("IN0"));
            w.connect(getInputPin<32>("EX_MEM_VALUE"),mux->getInputPin<32>("IN1"));
            w.connect(getInputPin<32>("MEM_WB_VALUE"),mux->getInputPin<32>("IN2"));
            w.connect(zero->getOutputPin<32>("OUT"),mux->getInputPin<32>("IN3"));
            w.connect(select->getOutputPin<2>("FORWARD_"+side),mux->getInputPin<2>("SEL"));
            w.connect(select->getOutputPin<2>("FORWARD_"+side),getOutputPin<2>("FORWARD_"+side));
            w.connect(mux->getOutputPin<32>("OUT"),getOutputPin<32>("VALUE_"+side));
        }
        w.finish();
    }
};
class ExecutePacketStatus : public IOComponent {
public:
    explicit ExecutePacketStatus(std::string name):IOComponent(std::move(name),[](IOComponent* s) {
        for(auto p:{"VALID","TARGET_MISALIGNED","BRANCH_TAKEN"}) s->addPin(p,PinType::INPUT);
        for(auto p:{"CONTROL","FETCH_STATUS"}) s->addPin<32>(p,PinType::INPUT);
        for(auto p:{"PRETERMINAL","REDIRECT","IS_BRANCH"}) s->addPin(p,PinType::OUTPUT);
    }) {}
    const char* getTypeName() const override {return "RV32IExecutePacketStatus";}
    void buildInternals(ComponentBuilder& b) override {
        circuit::memory_detail::StructuralWiring w(b);
        addControlUnpack(b,"CONTROL_FIELDS");addStatusUnpack(b,"FETCH_STATUS",{"PC_MISALIGNED","INSTRUCTION_FAULT"});
        auto c=b.getComponent<Rewire>("CONTROL_FIELDS"),f=b.getComponent<Rewire>("FETCH_STATUS");
        w.connect(getInputPin<32>("CONTROL"),c->getInputPin<32>("CONTROL"));
        w.connect(getInputPin<32>("FETCH_STATUS"),f->getInputPin<32>("STATUS"));
        auto branch=b.addNewComponent<BitSplitter<3>>("BRANCH_KIND");
        auto jump=b.addNewComponent<BitSplitter<2>>("JUMP_KIND");
        w.connect(c->getOutputPin<3>("BRANCH_TYPE"),branch->getInputPin<3>("IN"));
        w.connect(c->getOutputPin<2>("JUMP_TYPE"),jump->getInputPin<2>("IN"));
        auto is_branch=w.reduce("IS_BRANCH",{branch->getOutputPin("OUT_0"),branch->getOutputPin("OUT_1"),branch->getOutputPin("OUT_2")},false);
        w.connect(is_branch,getOutputPin("IS_BRANCH"));
        auto terminal=w.both("PRETERMINAL",getInputPin("VALID"),w.reduce("TERMINAL_REASON",{
            w.invert("NOT_LEGAL",c->getOutputPin("LEGAL")),c->getOutputPin("HALT_REQUEST"),c->getOutputPin("TRAP_REQUEST"),
            f->getOutputPin("PC_MISALIGNED"),f->getOutputPin("INSTRUCTION_FAULT"),getInputPin("TARGET_MISALIGNED")},false));
        w.connect(terminal,getOutputPin("PRETERMINAL"));
        w.connect(w.reduce("REDIRECT",{getInputPin("VALID"),w.invert("NOT_TERMINAL",terminal),
            w.reduce("RAW_REDIRECT",{getInputPin("BRANCH_TAKEN"),jump->getOutputPin("OUT_0"),jump->getOutputPin("OUT_1")},false)},true),getOutputPin("REDIRECT"));
        w.finish();
    }
};
}
void RV32IExecuteStage::buildInternals(ComponentBuilder& b) {
    circuit::memory_detail::StructuralWiring w(b);
    auto forward=b.addNewComponent<ForwardedOperands>("FORWARDED_OPERANDS");
    auto datapath=b.add(circuit::families::RV32ISingleCycleExecuteStage,"DATAPATH");
    auto next=b.add(circuit::families::RV32ISingleCycleNextPCStage,"NEXT_PC");
    auto pc4=b.addNewComponent<IncrementBy4>("PC_PLUS_4");
    auto status=b.addNewComponent<ExecutePacketStatus>("PACKET_STATUS");
    addControlUnpack(b,"CONTROL_FIELDS");addAddressUnpack(b,"REGISTER_FIELDS");
    addStatusPack(b,"EXECUTION_STATUS",{"TARGET_MISALIGNED"});
    auto ctrl=b.getComponent<Rewire>("CONTROL_FIELDS"),addr=b.getComponent<Rewire>("REGISTER_FIELDS"),exec=b.getComponent<Rewire>("EXECUTION_STATUS");
    auto pass=addPassThrough(b,"PACKET_PASS",{{"VALID",1},{"PC",32},{"INSTRUCTION",32},{"CONTROL",32},{"REGISTER_ADDRESSES",32},{"FETCH_STATUS",32}});
    for(auto& [name,pin]:pass->getAllInputPins()) {
        w.connect(getInputPinDynamic(name),pin);
        const auto output="OUT_"+name.substr(3);
        w.connect(pass->getOutputPinDynamic(output),getOutputPinDynamic(output));
    }
    w.connect(getInputPin<32>("IN_CONTROL"),ctrl->getInputPin<32>("CONTROL"));
    w.connect(getInputPin<32>("IN_REGISTER_ADDRESSES"),addr->getInputPin<32>("ADDRESSES"));
    for(const std::string p:{"RS1_VALUE","RS2_VALUE"}) w.connect(getInputPin<32>("IN_"+p),forward->getInputPin<32>(p));
    for(auto p:{"EX_MEM_VALID","EX_MEM_RD","EX_MEM_REG_WRITE","EX_MEM_RESULT_READY","EX_MEM_VALUE","MEM_WB_VALID","MEM_WB_RD","MEM_WB_REG_WRITE","MEM_WB_VALUE"})
        w.connect(getInputPinDynamic(p),forward->getInputPinDynamic(p));
    w.connect(addr->getOutputPin<5>("RS1"),forward->getInputPin<5>("ID_EX_RS1"));
    w.connect(addr->getOutputPin<5>("RS2"),forward->getInputPin<5>("ID_EX_RS2"));
    w.connect(forward->getOutputPin<32>("VALUE_A"),datapath->getInputPin<32>("RS1_VALUE"));
    w.connect(forward->getOutputPin<32>("VALUE_B"),datapath->getInputPin<32>("RS2_VALUE"));
    w.connect(forward->getOutputPin<32>("VALUE_B"),getOutputPin<32>("OUT_STORE_DATA"));
    w.connect(getInputPin<32>("IN_PC"),datapath->getInputPin<32>("PC"));
    w.connect(getInputPin<32>("IN_IMMEDIATE"),datapath->getInputPin<32>("IMM"));
    for(auto p:{"ALU_OP","ALU_A_SEL","ALU_B_SEL","BRANCH_TYPE","JUMP_TYPE"}) w.connect(ctrl->getOutputPinDynamic(p),datapath->getInputPinDynamic(p));
    w.connect(getInputPin<32>("IN_PC"),pc4->getInputPin<32>("A"));
    w.connect(pc4->getOutputPin<32>("Sum"),next->getInputPin<32>("PC_PLUS_4"));
    w.connect(pc4->getOutputPin<32>("Sum"),getOutputPin<32>("OUT_PC_PLUS_4"));
    for(auto p:{"CONTROL_TARGET","BRANCH_TAKEN"}) w.connect(datapath->getOutputPinDynamic(p),next->getInputPinDynamic(p));
    w.connect(ctrl->getOutputPin<2>("JUMP_TYPE"),next->getInputPin<2>("JUMP_TYPE"));
    w.connect(datapath->getOutputPin<32>("ALU_RESULT"),getOutputPin<32>("OUT_ALU_RESULT"));
    w.connect(datapath->getOutputPin("BRANCH_TAKEN"),getOutputPin("BRANCH_TAKEN"));
    w.connect(datapath->getOutputPin<32>("CONTROL_TARGET"),getOutputPin<32>("CONTROL_TARGET"));
    auto jump_bits=b.addNewComponent<BitSplitter<2>>("JUMP_DECISION");
    w.connect(ctrl->getOutputPin<2>("JUMP_TYPE"),jump_bits->getInputPin<2>("IN"));
    w.connect(w.reduce("CONTROL_TAKEN",{datapath->getOutputPin("BRANCH_TAKEN"),
        jump_bits->getOutputPin("OUT_0"),jump_bits->getOutputPin("OUT_1")},false),getOutputPin("CONTROL_TAKEN"));
    for(auto p:{"OUT_NEXT_PC","REDIRECT_PC"}) w.connect(next->getOutputPin<32>("NEXT_PC"),getOutputPin<32>(p));
    w.connect(next->getOutputPin("TARGET_MISALIGNED"),exec->getInputPin("TARGET_MISALIGNED"));
    w.connect(exec->getOutputPin<32>("STATUS"),getOutputPin<32>("OUT_EXECUTION_STATUS"));
    w.connect(getInputPin("IN_VALID"),status->getInputPin("VALID"));
    w.connect(getInputPin<32>("IN_CONTROL"),status->getInputPin<32>("CONTROL"));
    w.connect(getInputPin<32>("IN_FETCH_STATUS"),status->getInputPin<32>("FETCH_STATUS"));
    w.connect(next->getOutputPin("TARGET_MISALIGNED"),status->getInputPin("TARGET_MISALIGNED"));
    w.connect(datapath->getOutputPin("BRANCH_TAKEN"),status->getInputPin("BRANCH_TAKEN"));
    for(auto p:{"PRETERMINAL","REDIRECT","IS_BRANCH"}) w.connect(status->getOutputPin(p),getOutputPin(p));
    w.finish();
}
