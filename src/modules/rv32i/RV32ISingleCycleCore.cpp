#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "modules/rv32i/RV32IReferenceCore.hpp"
#include "modules/rv32i/RV32IMemoryRequest.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "RV32IZeroWaitMemoryPort.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include <limits>

namespace {
void initializeCorePins(IOComponent* s) {
    for(auto p:{"CLK","RST","ENABLE"}) s->addPin(p,PinType::INPUT);
    rv32i::memoryRequestPins(s,"IMEM_");rv32i::memoryRequestPins(s,"DMEM_");
    s->addPin<32>("PC",PinType::OUTPUT);
    for(auto p:{"HALTED","TRAPPED","INSTRUCTION_ATTEMPT"}) s->addPin(p,PinType::OUTPUT);
    s->addPin<4>("TRAP_CAUSE",PinType::OUTPUT);
}
}
namespace circuit::families {
const ComponentFamily RV32ISingleCycleCore{
    "rv32i.core.educational-single-cycle","RV32ISingleCycleCore",initializeCorePins,
    [](const std::string& n,const std::shared_ptr<BuildContext>& c) {
        return Component::createWithContext<::RV32ISingleCycleCore>(c,n);},
    [](const std::string& n,const std::shared_ptr<BuildContext>& c) {
        return Component::createWithContext<::RV32IReferenceCore>(c,n);}};
}
RV32ISingleCycleCore::RV32ISingleCycleCore(std::string n)
    : IOComponent(std::move(n),initializeCorePins) {}

void RV32ISingleCycleCore::buildInternals(ComponentBuilder& b) {
    using namespace circuit::families;
    fetch_=b.add(circuit::families::RV32ISingleCycleFetchStage,"FETCH");
    decode_=b.add(circuit::families::RV32ISingleCycleDecodeStage,"DECODE",{}, {"register-state-view"});
    execute_=b.add(circuit::families::RV32ISingleCycleExecuteStage,"EXECUTE");
    memory_=b.add(circuit::families::RV32ISingleCycleMemoryStage,"MEMORY");
    writeback_=b.add(circuit::families::RV32ISingleCycleWritebackStage,"WRITEBACK");
    next_pc_=b.add(circuit::families::RV32ISingleCycleNextPCStage,"NEXT_PC");
    execution_status_=b.add(RV32IExecutionStatus,"EXECUTION_STATUS");
    circuit::memory_detail::StructuralWiring w(b);
    const auto link=[&](const std::shared_ptr<IOComponent>& from,const char* output,
                        const std::shared_ptr<IOComponent>& to,const char* input) {
        w.connect(from->getOutputPinDynamic(output),to->getInputPinDynamic(input));
    };
    for(auto c:{fetch_,decode_,execution_status_}) for(auto p:{"CLK","RST"})
        w.connect(getInputPin(p),c->getInputPin(p));
    w.connect(getInputPin("ENABLE"),execution_status_->getInputPin("ENABLE"));
    rv32i::wireZeroWaitPort(b,w,*this,fetch_,"IMEM_",true);
    rv32i::wireZeroWaitPort(b,w,*this,memory_,"DMEM_",false);
    w.connect(fetch_->getOutputPin<32>("PC"),getOutputPin<32>("PC"));
    link(fetch_,"INSTRUCTION",decode_,"INSTRUCTION");
    link(fetch_,"PC",execute_,"PC");
    link(fetch_,"PC_PLUS_4",writeback_,"PC_PLUS_4");
    link(fetch_,"PC_PLUS_4",next_pc_,"PC_PLUS_4");
    link(fetch_,"PC_MISALIGNED",execution_status_,"PC_MISALIGNED");
    link(fetch_,"DONE",execution_status_,"IMEM_READY");
    link(fetch_,"FAULT",execution_status_,"IMEM_FAULT");
    for(auto p:{"RS1_VALUE","RS2_VALUE","IMM","ALU_OP","ALU_A_SEL","ALU_B_SEL","BRANCH_TYPE","JUMP_TYPE"})
        link(decode_,p,execute_,p);
    for(auto p:{"LEGAL","REG_WRITE","MEM_READ","MEM_WRITE","MEM_SIZE","HALT_REQUEST","TRAP_REQUEST","DECODE_TRAP_CAUSE"})
        link(decode_,p,execution_status_,p);
    link(decode_,"JUMP_TYPE",next_pc_,"JUMP_TYPE");
    link(decode_,"WRITEBACK_SEL",writeback_,"WRITEBACK_SEL");
    link(decode_,"RS2_VALUE",memory_,"WRITE_DATA");
    link(decode_,"MEM_WRITE",memory_,"MEM_WRITE");
    link(decode_,"MEM_SIZE",memory_,"SIZE");
    link(decode_,"LOAD_SIGN_EXTEND",memory_,"SIGN_EXTEND");
    link(execute_,"ALU_RESULT",memory_,"ADDR");
    link(execute_,"ALU_RESULT",execution_status_,"ALU_ADDRESS");
    link(execute_,"ALU_RESULT",writeback_,"ALU_RESULT");
    link(execute_,"BRANCH_TAKEN",next_pc_,"BRANCH_TAKEN");
    link(execute_,"CONTROL_TARGET",next_pc_,"CONTROL_TARGET");
    link(memory_,"READ_DATA",writeback_,"READ_DATA");
    link(memory_,"DONE",execution_status_,"DMEM_READY");
    link(memory_,"FAULT",execution_status_,"DMEM_FAULT");
    link(writeback_,"WB_VALUE",decode_,"WB_VALUE");
    link(next_pc_,"NEXT_PC",fetch_,"NEXT_PC");
    link(next_pc_,"TARGET_MISALIGNED",execution_status_,"TARGET_MISALIGNED");
    link(execution_status_,"PC_WRITE",fetch_,"PC_WRITE");
    link(execution_status_,"REGISTER_WRITE",decode_,"REGISTER_WRITE");
    link(execution_status_,"FETCH_REQUEST_ACTIVE",fetch_,"ISSUE");
    link(execution_status_,"MEMORY_REQUEST_ACTIVE",memory_,"ISSUE");
    // PC, register file, and data memory all commit on the same edge. No
    // request or response registers exist between these combinational stages.
    // ISSUE already excludes older fetch/decode faults and terminal state.
    // Alignment is checked by the memory stage; the endpoint validates the
    // store and suppresses invalid writes itself. Never feed its response back
    // into REQ_OP: a rejected write must remain a write, not become a read.
    w.connect(w.constant("STORE_AUTHORIZED",true),memory_->getInputPin("COMMIT"));
    for(auto p:{"HALTED","TRAPPED","TRAP_CAUSE","INSTRUCTION_ATTEMPT"})
        w.connect(execution_status_->getOutputPinDynamic(p),getOutputPinDynamic(p));
    w.finish();
}

rv32i::RV32IArchitecturalState RV32ISingleCycleCore::snapshotArchitecturalState() const {
    if(!fetch_||!decode_||!execution_status_) throw std::logic_error("RV32I core is not built");
    rv32i::RV32IArchitecturalState state;
    state.pc=fetch_->getOutputPin<32>("PC")->getValueAsVector();
    auto view=std::dynamic_pointer_cast<RegisterStateView>(decode_);
    if(!view) throw std::logic_error("Decode stage lacks register-state-view");
    const auto registers=view->getRegisterStateAtTime(std::numeric_limits<size_t>::max());
    for(size_t i=0;i<state.x.size()&&i<registers.size();++i) state.x[i]=registers[i];
    state.halted=execution_status_->getOutputPin("HALTED")->getValue();
    state.trapped=execution_status_->getOutputPin("TRAPPED")->getValue();
    state.trap_cause=execution_status_->getOutputPin<4>("TRAP_CAUSE")->getValueAsVector();
    return state;
}
