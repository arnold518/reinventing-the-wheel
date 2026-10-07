#include "RV32IBaselinePipelineInternal.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "RV32IZeroWaitMemoryPort.hpp"
#include "RV32IPipelineAssembly.hpp"
#include "modules/rv32i/RV32IPipelineRegisters.hpp"
#include "modules/rv32i/RV32IPipelineStages.hpp"
#include "modules/rv32i/RV32IPipelineObservation.hpp"
#include <limits>

using namespace rv32i::baseline;
using circuit::detail::StructuralWiring;
namespace {
void corePins(IOComponent* s) {
    for(auto p:{"CLK","RST","ENABLE"}) s->addPin(p,PinType::INPUT);
    rv32i::memoryRequestPins(s,"IMEM_");rv32i::memoryRequestPins(s,"DMEM_");
    s->addPin<32>("FETCH_PC",PinType::OUTPUT);
    s->addPin<4>("TRAP_CAUSE",PinType::OUTPUT);
    for(auto p:{"HALTED","TRAPPED","INSTRUCTION_ATTEMPT","COMMIT_VALID",
            "PIPELINE_STALL","DATA_STALL","BRANCH_WAIT","PIPELINE_FLUSH",
            "LOAD_USE_STALL","MEMORY_STALL","DATA_PORT_STALL"}) s->addPin(p,PinType::OUTPUT);
    for(const std::string stage:{"IF_ID","ID_EX","EX_MEM","MEM_WB"}) {
        s->addPin(stage+"_VALID",PinType::OUTPUT);s->addPin<32>(stage+"_PC",PinType::OUTPUT);
    }
}
}
namespace circuit::families {
const ComponentFamily RV32IBaselinePipelineCore{"rv32i.core.baseline-pipeline","RV32IBaselinePipelineCore",corePins,
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::RV32IBaselinePipelineCore>(c,n);},
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return rv32i::baseline::directCore(n,c);}};
}
RV32IBaselinePipelineCore::RV32IBaselinePipelineCore(std::string n):IOComponent(std::move(n),corePins) {}
void RV32IBaselinePipelineCore::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto f=b.add(circuit::families::RV32IBaselineFetchStage,"FETCH");
    auto dreg = rv32i::detail::addCompactPipelineRegister<RV32IIFIDPipelineRegister>(
        b, circuit::families::RV32IIFIDPipelineRegister, "IF_ID");
    decode_=b.add(circuit::families::RV32IBaselineDecodeStage,"DECODE",{}, {"register-state-view"});
    auto ereg = rv32i::detail::addCompactPipelineRegister<RV32IIDEXPipelineRegister>(
        b, circuit::families::RV32IIDEXPipelineRegister, "ID_EX");
    auto e=b.add(circuit::families::RV32IBaselineExecuteStage,"EXECUTE");
    auto mreg = rv32i::detail::addCompactPipelineRegister<RV32IEXMEMPipelineRegister>(
        b, circuit::families::RV32IEXMEMPipelineRegister, "EX_MEM");
    auto m=b.add(circuit::families::RV32IBaselineMemoryStage,"MEMORY");
    auto wreg = rv32i::detail::addCompactPipelineRegister<RV32IMEMWBPipelineRegister>(
        b, circuit::families::RV32IMEMWBPipelineRegister, "MEM_WB");
    packet_=wreg;
    writeback_=b.add(circuit::families::RV32IWritebackStage,"WRITEBACK");
    execution_status_=b.add(circuit::families::RV32IPipelineExecutionStatus,"EXECUTION_STATUS");
    auto dependencies=b.add(circuit::families::RV32IBaselineDependencyUnit,"DEPENDENCIES");
    auto control=b.add(circuit::families::RV32IBaselineControl,"CONTROL");
    for(auto c:{f,dreg,decode_,ereg,mreg,wreg,execution_status_}) for(auto p:{"CLK","RST"})
        w.connect(getInputPin(p),c->getInputPin(p));
    for(auto p:{"RST","ENABLE"}) w.connect(getInputPin(p),control->getInputPin(p));
    // Alternating stages and registers are the complete top-level datapath.
    const auto link=[&](auto stage,auto reg,auto next,const auto& fields,const std::string& name) {
        w.connect(stage->getOutputPin("OUT_VALID"),reg->getInputPin("D_VALID"));
        w.connect(reg->getOutputPin("Q_VALID"),next->getInputPin("IN_VALID"));
        w.connect(reg->getOutputPin("Q_VALID"),getOutputPin(name+"_VALID"));
        w.connect(reg->template getOutputPin<32>("Q_PC"),getOutputPin<32>(name+"_PC"));
        for(auto& p:fields) {
            w.connect(stage->getOutputPinDynamic("OUT_"+p),reg->getInputPinDynamic("D_"+p));
            if(next->getAllInputPins().contains("IN_"+p))
                w.connect(reg->getOutputPinDynamic("Q_"+p),next->getInputPinDynamic("IN_"+p));
        }
        w.connect(control->getOutputPin(name+"_WRITE"),reg->getInputPin("WRITE_ENABLE"));
        w.connect(control->getOutputPin(name+"_FLUSH"),reg->getInputPin("FLUSH"));
    };
    link(f,dreg,decode_,fetchFields,"IF_ID");link(decode_,ereg,e,decodeFields,"ID_EX");
    link(e,mreg,m,executeFields,"EX_MEM");link(m,wreg,writeback_,memoryFields,"MEM_WB");
    for(auto p:{"PC_WRITE","PC_REDIRECT","FETCH_VALID"}) w.connect(control->getOutputPin(p),f->getInputPin(p));
    w.connect(e->getOutputPin<32>("OUT_NEXT_PC"),f->getInputPin<32>("REDIRECT_PC"));
    w.connect(f->getOutputPin<32>("FETCH_PC"),getOutputPin<32>("FETCH_PC"));
    w.connect(control->getOutputPin("FETCH_VALID"),getOutputPin("INSTRUCTION_ATTEMPT"));
    for(auto p:{"WB_RD","WB_VALUE","REGISTER_WRITE"}) w.connect(writeback_->getOutputPinDynamic(p),decode_->getInputPinDynamic(p));
    for(auto p:{"HALT_EVENT","TRAP_EVENT"})
        w.connect(writeback_->getOutputPin(p),execution_status_->getInputPin(p));
    w.connect(writeback_->getOutputPin<4>("TRAP_CAUSE"),execution_status_->getInputPin<4>("EVENT_TRAP_CAUSE"));
    for(auto p:{"HALTED","TRAPPED"}) w.connect(execution_status_->getOutputPin(p),control->getInputPin(p));
    for(auto p:{"HALTED","TRAPPED","TRAP_CAUSE"})
        w.connect(execution_status_->getOutputPinDynamic(p),getOutputPinDynamic(p));
    for(auto& [p,pin]:getAllOutputPins()) if(p!="TRAP_CAUSE" && writeback_->getAllOutputPins().contains(p))
        w.connect(writeback_->getOutputPinDynamic(p),pin);
    w.connect(control->getOutputPin("COMMIT_ENABLE"),writeback_->getInputPin("COMMIT_ENABLE"));
    w.connect(control->getOutputPin("MEMORY_ENABLE"),m->getInputPin("MEMORY_ENABLE"));
    for(auto& [stage,c]:std::vector<std::pair<std::string,std::shared_ptr<IOComponent>>>{{"D",decode_},{"E",e},{"M",m},{"W",writeback_}})
        w.connect(c->getOutputPin("TERMINAL"),control->getInputPin(stage+"_TERMINAL"));
    w.connect(decode_->getOutputPin("CONTROL_FLOW"),control->getInputPin("D_CONTROL_FLOW"));
    w.connect(e->getOutputPin("CONTROL_FLOW"),control->getInputPin("E_CONTROL_FLOW"));
    w.connect(dreg->getOutputPin("Q_VALID"),dependencies->getInputPin("D_VALID"));
    for(const std::string p:{"CONTROL","REGISTER_ADDRESSES"})
        w.connect(decode_->getOutputPin<32>("OUT_"+p),dependencies->getInputPin<32>("D_"+p));
    for(auto& [stage,reg]:std::vector<std::pair<std::string,std::shared_ptr<IOComponent>>>{{"E",ereg},{"M",mreg},{"W",wreg}}) {
        w.connect(reg->getOutputPin("Q_VALID"),dependencies->getInputPin(stage+"_VALID"));
        for(const std::string p:{"CONTROL","REGISTER_ADDRESSES"})
            w.connect(reg->getOutputPin<32>("Q_"+p),dependencies->getInputPin<32>(stage+"_"+p));
    }
    w.connect(dependencies->getOutputPin("DATA_HAZARD"),control->getInputPin("DATA_HAZARD"));
    for(auto p:{"DATA_STALL","BRANCH_WAIT","PIPELINE_FLUSH"}) w.connect(control->getOutputPin(p),getOutputPin(p));
    w.connect(control->getOutputPin("DATA_STALL"),getOutputPin("PIPELINE_STALL"));
    // Legacy metric pins stay zero: this baseline has neither memory waits nor
    // a load-only interlock. DATA_STALL is the general RAW-dependency counter.
    auto low=w.constant("NO_MEMORY_WAIT",false);
    for(auto p:{"LOAD_USE_STALL","MEMORY_STALL","DATA_PORT_STALL"}) w.connect(low,getOutputPin(p));
    rv32i::wireZeroWaitPort(b,w,*this,f,"IMEM_",true);
    rv32i::wireZeroWaitPort(b,w,*this,m,"DMEM_",false);
    w.finish();
}
rv32i::RV32IArchitecturalState RV32IBaselinePipelineCore::snapshotPipelineState(uint32_t observed_pc) const {
    rv32i::RV32IArchitecturalState state;
    state.pc=rv32i::RV32IArchitecturalState::fromKnown(rv32i::RV32IState{.pc=observed_pc}).pc;
    const auto regs=std::dynamic_pointer_cast<RegisterStateView>(decode_)->getRegisterStateAtTime(std::numeric_limits<size_t>::max());
    for(size_t i=0;i<32;++i) state.x[i]=regs[i];
    state.halted=execution_status_->getOutputPin("HALTED")->getValue();state.trapped=execution_status_->getOutputPin("TRAPPED")->getValue();
    state.trap_cause=execution_status_->getOutputPin<4>("TRAP_CAUSE")->getValueAsVector();return state;
}

RV32IRetirementObservation RV32IBaselinePipelineCore::pendingRetirement() const {
    return rv32i::observeRetirementPacket(*packet_, writeback_->getOutputPin("TERMINAL")->getValue()==LogicValue::HIGH);
}
