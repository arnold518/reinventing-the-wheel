#include "components/StructuralNetBuilder.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "modules/rv32i/RV32IPrediction.hpp"
#include "components/selection/BuildContext.hpp"
#include "RV32IMemoryRequestWiring.hpp"
#include "RV32IPipelineAssembly.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "modules/rv32i/RV32IFiveStageCoreBehavioral.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPipelineRegisters.hpp"
#include "modules/rv32i/RV32IPipelineStages.hpp"
#include "modules/rv32i/RV32IPipelineObservation.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
void fiveStageCorePins(IOComponent* self) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("ENABLE", PinType::INPUT);
    rv32i::memoryRequestPins(self, "IMEM_");
    rv32i::memoryRequestPins(self, "DMEM_");

    self->addPin<32>("FETCH_PC", PinType::OUTPUT);
    self->addPin("HALTED", PinType::OUTPUT);
    self->addPin("TRAPPED", PinType::OUTPUT);
    self->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
    self->addPin("INSTRUCTION_ATTEMPT", PinType::OUTPUT);
    self->addPin("COMMIT_VALID", PinType::OUTPUT);


    self->addPin("PIPELINE_STALL", PinType::OUTPUT);
    self->addPin("LOAD_USE_STALL", PinType::OUTPUT);
    self->addPin("MEMORY_STALL", PinType::OUTPUT);
    self->addPin("DATA_PORT_STALL", PinType::OUTPUT);
    self->addPin("PIPELINE_FLUSH", PinType::OUTPUT);
    for(auto p:{"BRANCH_RESOLVED","BRANCH_MISS","PREDICTION_RECOVERY"}) self->addPin(p,PinType::OUTPUT);
    self->addPin("IF_ID_VALID", PinType::OUTPUT);
    self->addPin<32>("IF_ID_PC", PinType::OUTPUT);
    self->addPin("ID_EX_VALID", PinType::OUTPUT);
    self->addPin<32>("ID_EX_PC", PinType::OUTPUT);
    self->addPin("EX_MEM_VALID", PinType::OUTPUT);
    self->addPin<32>("EX_MEM_PC", PinType::OUTPUT);
    self->addPin("MEM_WB_VALID", PinType::OUTPUT);
    self->addPin<32>("MEM_WB_PC", PinType::OUTPUT);
}
} // namespace

namespace circuit::families {
const ComponentFamily RV32IFiveStageCore{
    "rv32i.core.educational-five-stage",
    "RV32IFiveStageCore",
    fiveStageCorePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IFiveStageCore>(
            context, name, context ? rv32i::predictionConfig(context->parameters()) : rv32i::PredictionConfig{});
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IFiveStageCoreBehavioral>(context, name, context ? rv32i::predictionConfig(context->parameters()) : rv32i::PredictionConfig{});
    }};
} // namespace circuit::families

RV32IFiveStageCore::RV32IFiveStageCore(std::string name, rv32i::PredictionConfig prediction)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IFiveStageCore.pinInitializer()), prediction_(prediction) { prediction_.validate(); }

void RV32IFiveStageCore::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    auto fetch_request=builder.addNewComponent<RV32IMemoryRequest>("FETCH_REQUEST", true, true);
    auto data_request=builder.addNewComponent<RV32IMemoryRequest>("DATA_REQUEST");
    auto lanes=builder.addNewComponent<RV32IMemoryLanes>("LOAD_STORE");
    auto data_op=builder.addNewComponent<BitJoiner<3>>("DATA_OPERATION");
    rv32i::wireRequestBus(builder,*this,fetch_request,"IMEM_");
    rv32i::wireRequestBus(builder,*this,data_request,"DMEM_");
    rv32i::wireRequestFields(builder,fetch_request,data_request,lanes,data_op,true);
    const auto fetch = rv32i::detail::addFamilyOrBehavioral<RV32IFetchStage>(
        builder, circuit::families::RV32IFetchStage, "FETCH", {{"external_next_pc", "1"}}, true);
    const auto if_id = rv32i::detail::addCompactPipelineRegister<RV32IIFIDPipelineRegister>(
        builder, circuit::families::RV32IIFIDPipelineRegister, "IF_ID");
    decode_stage_ = builder.add(
        circuit::families::RV32IDecodeStage, "DECODE");
    const auto id_ex = rv32i::detail::addCompactPipelineRegister<RV32IIDEXPipelineRegister>(
        builder, circuit::families::RV32IIDEXPipelineRegister, "ID_EX");
    const auto execute = builder.add(
        circuit::families::RV32IExecuteStage, "EXECUTE");
    const auto ex_mem = rv32i::detail::addCompactPipelineRegister<RV32IEXMEMPipelineRegister>(
        builder, circuit::families::RV32IEXMEMPipelineRegister, "EX_MEM");
    const auto memory = builder.add(
        circuit::families::RV32IMemoryStage, "MEMORY");
    const auto mem_wb = rv32i::detail::addCompactPipelineRegister<RV32IMEMWBPipelineRegister>(
        builder, circuit::families::RV32IMEMWBPipelineRegister, "MEM_WB");
    retirement_packet_ = mem_wb;
    writeback_stage_ = builder.add(
        circuit::families::RV32IWritebackStage, "WRITEBACK");
    execution_status_ = builder.add(circuit::families::RV32IPipelineExecutionStatus, "EXECUTION_STATUS");
    const auto coordinator = builder.add(
        circuit::families::RV32IPipelineCoordinator, "COORDINATOR");
    using PinPtr = std::shared_ptr<PinBase>;
    w.connect(coordinator->getOutputPinDynamic("FETCH_RESPONSE_TAKE"),
        {fetch_request->getInputPinDynamic("TAKE")});
    w.connect(coordinator->getOutputPinDynamic("DATA_REQUEST_ISSUE"),
        {data_request->getInputPinDynamic("ISSUE")});
    const auto prediction = rv32i::detail::addFamilyOrBehavioral<RV32IPredictionPath>(
        builder, circuit::families::RV32IPredictionPath, "PREDICTION", prediction_.parameters(), prediction_);
    w.connect(prediction->getOutputPinDynamic("FETCH_NEXT_PC"),
        {fetch->getInputPinDynamic("REDIRECT_PC")});
    w.connect(execute->getOutputPinDynamic("IS_BRANCH"),
        {prediction->getInputPinDynamic("EX_IS_BRANCH")});
    w.connect(execute->getOutputPinDynamic("BRANCH_TAKEN"),
        {prediction->getInputPinDynamic("EX_TAKEN")});
    w.connect(prediction->getOutputPinDynamic("UPDATE"),
        {getOutputPinDynamic("BRANCH_RESOLVED")});
    w.connect(prediction->getOutputPinDynamic("BRANCH_MISS"),{getOutputPinDynamic("BRANCH_MISS")});
    w.connect(prediction->getOutputPinDynamic("RECOVERY"),
        {getOutputPinDynamic("PREDICTION_RECOVERY")});

    for (const auto* pin : {"CLK", "RST"}) {
        std::vector<PinPtr> sinks;
        for (const auto& clocked : std::vector<std::shared_ptr<IOComponent>>{
                 prediction, fetch_request, data_request, fetch, if_id,
                 decode_stage_, id_ex, ex_mem, mem_wb, execution_status_})
            sinks.push_back(clocked->getInputPinDynamic(pin));
        w.connect(getInputPinDynamic(pin), sinks);
    }
    w.connect(getInputPinDynamic("ENABLE"), {coordinator->getInputPinDynamic("ENABLE")});
    w.connect(fetch_request->getOutputPinDynamic("READ_DATA"),
        {fetch->getInputPinDynamic("IMEM_READ_DATA"), prediction->getInputPinDynamic("FETCH_INSTRUCTION")});
    w.connect(fetch_request->getOutputPinDynamic("DONE"),
        {coordinator->getInputPinDynamic("IMEM_READY")});
    w.connect(fetch_request->getOutputPinDynamic("FAULT"),
        {fetch->getInputPinDynamic("IMEM_FAULT")});
    w.connect(lanes->getOutputPinDynamic("READ_DATA"),
        {memory->getInputPinDynamic("DMEM_READ_DATA")});
    w.connect(data_request->getOutputPinDynamic("DONE"),
        {coordinator->getInputPinDynamic("DMEM_READY")});
    w.connect(data_request->getOutputPinDynamic("FAULT"),
        {memory->getInputPinDynamic("DMEM_FAULT")});

    w.connect(fetch->getOutputPinDynamic("FETCH_PC"),
        {getOutputPinDynamic("FETCH_PC"), prediction->getInputPinDynamic("FETCH_PC")});
    w.connect(fetch->getOutputPinDynamic("IMEM_ADDR"), {fetch_request->getInputPinDynamic("ADDR")});
    w.connect(fetch->getOutputPinDynamic("OUT_VALID"), {if_id->getInputPinDynamic("D_VALID")});
    for (const auto* field : {
             "PC", "INSTRUCTION", "FETCH_STATUS"}) {
        w.connect(fetch->getOutputPinDynamic("OUT_" + std::string(field)),
            {if_id->getInputPinDynamic("D_" + std::string(field))});
    }

    w.connect(if_id->getOutputPinDynamic("Q_VALID"),
        {decode_stage_->getInputPinDynamic("IN_VALID"), getOutputPinDynamic("IF_ID_VALID")});
    w.connect(if_id->getOutputPinDynamic("Q_PC"),
        {decode_stage_->getInputPinDynamic("IN_PC"), getOutputPinDynamic("IF_ID_PC")});
    w.connect(if_id->getOutputPinDynamic("Q_INSTRUCTION"),
        {decode_stage_->getInputPinDynamic("IN_INSTRUCTION")});
    w.connect(if_id->getOutputPinDynamic("Q_FETCH_STATUS"),
        {decode_stage_->getInputPinDynamic("IN_FETCH_STATUS")});

    w.connect(decode_stage_->getOutputPinDynamic("OUT_VALID"),
        {id_ex->getInputPinDynamic("D_VALID")});
    for (const auto* field : {
             "PC", "INSTRUCTION", "RS1_VALUE", "RS2_VALUE",
             "IMMEDIATE", "CONTROL", "REGISTER_ADDRESSES",
             "FETCH_STATUS"}) {
        w.connect(decode_stage_->getOutputPinDynamic("OUT_" + std::string(field)),
            {id_ex->getInputPinDynamic("D_" + std::string(field))});
    }

    w.connect(id_ex->getOutputPinDynamic("Q_VALID"),
        {prediction->getInputPinDynamic("EX_VALID"), execute->getInputPinDynamic("IN_VALID"),
         decode_stage_->getInputPinDynamic("EX_VALID"), getOutputPinDynamic("ID_EX_VALID")});
    w.connect(id_ex->getOutputPinDynamic("Q_PC"),
        {prediction->getInputPinDynamic("EX_PC"), execute->getInputPinDynamic("IN_PC"),
         getOutputPinDynamic("ID_EX_PC")});
    for (const auto* field : {
             "INSTRUCTION", "RS1_VALUE", "RS2_VALUE", "IMMEDIATE",
             "FETCH_STATUS"}) {
        w.connect(id_ex->getOutputPinDynamic("Q_" + std::string(field)),
            {execute->getInputPinDynamic("IN_" + std::string(field))});
    }
    w.connect(id_ex->getOutputPinDynamic("Q_CONTROL"),
        {execute->getInputPinDynamic("IN_CONTROL"), decode_stage_->getInputPinDynamic("EX_CONTROL")});
    w.connect(id_ex->getOutputPinDynamic("Q_REGISTER_ADDRESSES"),
        {execute->getInputPinDynamic("IN_REGISTER_ADDRESSES"),
         decode_stage_->getInputPinDynamic("EX_REGISTER_ADDRESSES")});

    w.connect(execute->getOutputPinDynamic("OUT_VALID"),
        {ex_mem->getInputPinDynamic("D_VALID")});
    for (const auto* field : {
             "PC", "INSTRUCTION", "ALU_RESULT", "STORE_DATA",
             "PC_PLUS_4", "NEXT_PC", "CONTROL",
             "REGISTER_ADDRESSES", "FETCH_STATUS",
             "EXECUTION_STATUS"}) {
        std::vector<PinPtr> sinks{ex_mem->getInputPinDynamic("D_"+std::string(field))};
        if(std::string(field)=="PC_PLUS_4") sinks.push_back(prediction->getInputPinDynamic("EX_FALLTHROUGH_PC"));
        w.connect(execute->getOutputPinDynamic("OUT_" + std::string(field)),
            sinks);
    }
    w.connect(prediction->getOutputPinDynamic("MISPREDICT"),
        {coordinator->getInputPinDynamic("EX_REDIRECT")});
    w.connect(execute->getOutputPinDynamic("CONTROL_TARGET"),
        {prediction->getInputPinDynamic("EX_TARGET_PC")});
    w.connect(execute->getOutputPinDynamic("CONTROL_TAKEN"),
        {prediction->getInputPinDynamic("EX_CONTROL_TAKEN")});
    w.connect(execute->getOutputPinDynamic("PRETERMINAL"),
        {coordinator->getInputPinDynamic("EX_PRETERMINAL"), prediction->getInputPinDynamic("EX_PRETERMINAL")});

    w.connect(ex_mem->getOutputPinDynamic("Q_VALID"),
        {memory->getInputPinDynamic("IN_VALID"), execute->getInputPinDynamic("EX_MEM_VALID"),
         getOutputPinDynamic("EX_MEM_VALID")});
    w.connect(ex_mem->getOutputPinDynamic("Q_PC"),
        {memory->getInputPinDynamic("IN_PC"), getOutputPinDynamic("EX_MEM_PC")});
    for (const auto* field : {
             "INSTRUCTION", "ALU_RESULT", "STORE_DATA", "PC_PLUS_4",
             "NEXT_PC", "CONTROL", "REGISTER_ADDRESSES",
             "FETCH_STATUS", "EXECUTION_STATUS"}) {
        w.connect(ex_mem->getOutputPinDynamic("Q_" + std::string(field)),
            {memory->getInputPinDynamic("IN_" + std::string(field))});
    }

    w.connect(memory->getOutputPinDynamic("OUT_VALID"),
        {mem_wb->getInputPinDynamic("D_VALID")});
    for (const auto* field : {
             "PC", "INSTRUCTION", "ALU_RESULT", "MEMORY_DATA",
             "STORE_DATA", "PC_PLUS_4", "NEXT_PC", "CONTROL",
             "REGISTER_ADDRESSES", "FETCH_STATUS",
             "EXECUTION_STATUS", "MEMORY_STATUS"}) {
        w.connect(memory->getOutputPinDynamic("OUT_" + std::string(field)),
            {mem_wb->getInputPinDynamic("D_" + std::string(field))});
    }
    w.connect(memory->getOutputPinDynamic("LOAD_REQUEST"),
        {coordinator->getInputPinDynamic("MEM_LOAD")});
    w.connect(memory->getOutputPinDynamic("STORE_REQUEST"),
        {coordinator->getInputPinDynamic("MEM_STORE")});
    w.connect(memory->getOutputPinDynamic("MEMORY_FAULT"),
        {coordinator->getInputPinDynamic("MEMORY_FAULT")});
    w.connect(memory->getOutputPinDynamic("PRETERMINAL"),
        {coordinator->getInputPinDynamic("MEMORY_TERMINAL")});
    w.connect(memory->getOutputPinDynamic("FORWARD_RD"),
        {execute->getInputPinDynamic("EX_MEM_RD")});
    w.connect(memory->getOutputPinDynamic("FORWARD_REG_WRITE"),
        {execute->getInputPinDynamic("EX_MEM_REG_WRITE")});
    w.connect(memory->getOutputPinDynamic("FORWARD_RESULT_READY"),
        {execute->getInputPinDynamic("EX_MEM_RESULT_READY")});
    w.connect(memory->getOutputPinDynamic("FORWARD_VALUE"),
        {execute->getInputPinDynamic("EX_MEM_VALUE")});

    w.connect(mem_wb->getOutputPinDynamic("Q_VALID"),
        {writeback_stage_->getInputPinDynamic("IN_VALID"), getOutputPinDynamic("MEM_WB_VALID")});
    w.connect(mem_wb->getOutputPinDynamic("Q_PC"), {getOutputPinDynamic("MEM_WB_PC")});
    for (const auto* field : {
             "ALU_RESULT", "MEMORY_DATA",
             "PC_PLUS_4", "CONTROL",
             "REGISTER_ADDRESSES", "FETCH_STATUS",
             "EXECUTION_STATUS", "MEMORY_STATUS"}) {
        w.connect(mem_wb->getOutputPinDynamic("Q_" + std::string(field)),
            {writeback_stage_->getInputPinDynamic("IN_" + std::string(field))});
    }

    w.connect(writeback_stage_->getOutputPinDynamic("WB_VALID"),
        {decode_stage_->getInputPinDynamic("WB_VALID"), execute->getInputPinDynamic("MEM_WB_VALID")});
    w.connect(writeback_stage_->getOutputPinDynamic("WB_REG_WRITE"),
        {decode_stage_->getInputPinDynamic("WB_REG_WRITE"), execute->getInputPinDynamic("MEM_WB_REG_WRITE")});
    w.connect(writeback_stage_->getOutputPinDynamic("REGISTER_WRITE"),
        {decode_stage_->getInputPinDynamic("WB_REGISTER_WRITE")});
    w.connect(writeback_stage_->getOutputPinDynamic("WB_RD"),
        {decode_stage_->getInputPinDynamic("WB_RD"), execute->getInputPinDynamic("MEM_WB_RD")});
    w.connect(writeback_stage_->getOutputPinDynamic("WB_VALUE"),
        {decode_stage_->getInputPinDynamic("WB_VALUE"), execute->getInputPinDynamic("MEM_WB_VALUE")});
    w.connect(writeback_stage_->getOutputPinDynamic("TERMINAL"),
        {coordinator->getInputPinDynamic("WB_TERMINAL")});
    w.connect(writeback_stage_->getOutputPinDynamic("RETIRE_TERMINAL"),
        {coordinator->getInputPinDynamic("RETIRE_TERMINAL")});
    w.connect(execution_status_->getOutputPinDynamic("HALTED"),
        {coordinator->getInputPinDynamic("HALTED"), getOutputPinDynamic("HALTED")});
    w.connect(execution_status_->getOutputPinDynamic("TRAPPED"),
        {coordinator->getInputPinDynamic("TRAPPED"), getOutputPinDynamic("TRAPPED")});

    w.connect(decode_stage_->getOutputPinDynamic("TERMINAL"),
        {coordinator->getInputPinDynamic("TERMINAL_PENDING")});
    w.connect(decode_stage_->getOutputPinDynamic("LOAD_USE_HAZARD"),
        {coordinator->getInputPinDynamic("LOAD_USE_HAZARD")});

    w.connect(coordinator->getOutputPinDynamic("FETCH_VALID"),
        {fetch->getInputPinDynamic("FETCH_VALID")});
    w.connect(coordinator->getOutputPinDynamic("FETCH_PC_WRITE"),
        {fetch->getInputPinDynamic("PC_WRITE")});
    w.connect(coordinator->getOutputPinDynamic("FETCH_PC_REDIRECT"),
        {prediction->getInputPinDynamic("REDIRECT_APPLY")});
    w.connect(coordinator->getOutputPinDynamic("IF_ID_WRITE"),
        {if_id->getInputPinDynamic("WRITE_ENABLE"), prediction->getInputPinDynamic("IF_ID_WRITE")});
    w.connect(coordinator->getOutputPinDynamic("IF_ID_FLUSH"),
        {if_id->getInputPinDynamic("FLUSH")});
    w.connect(coordinator->getOutputPinDynamic("ID_EX_WRITE"),
        {id_ex->getInputPinDynamic("WRITE_ENABLE"), prediction->getInputPinDynamic("ID_EX_WRITE")});
    w.connect(coordinator->getOutputPinDynamic("ID_EX_FLUSH"),
        {id_ex->getInputPinDynamic("FLUSH")});
    w.connect(coordinator->getOutputPinDynamic("EX_MEM_WRITE"),
        {ex_mem->getInputPinDynamic("WRITE_ENABLE"), data_request->getInputPinDynamic("TAKE"),
         prediction->getInputPinDynamic("EX_ADVANCE")});
    w.connect(coordinator->getOutputPinDynamic("EX_MEM_FLUSH"),
        {ex_mem->getInputPinDynamic("FLUSH"), prediction->getInputPinDynamic("EX_KILL")});
    w.connect(coordinator->getOutputPinDynamic("MEM_WB_WRITE"),
        {mem_wb->getInputPinDynamic("WRITE_ENABLE")});
    w.connect(coordinator->getOutputPinDynamic("MEM_WB_FLUSH"),
        {mem_wb->getInputPinDynamic("FLUSH")});
    w.connect(coordinator->getOutputPinDynamic("COMMIT_ENABLE"),
        {writeback_stage_->getInputPinDynamic("COMMIT_ENABLE")});
    // Speculative instruction reads are side-effect-free. Only registered
    // halt/trap state and ENABLE gate issue; late terminal/redirect logic
    // gates admission, not the instruction data and prediction datapath.
    w.connect(coordinator->getOutputPinDynamic("ACTIVE"),
        {fetch_request->getInputPinDynamic("ISSUE")});
    w.connect(coordinator->getOutputPinDynamic("DMEM_READ_ENABLE"),
        {memory->getInputPinDynamic("DMEM_READ_ENABLE")});
    w.connect(coordinator->getOutputPinDynamic("DMEM_WRITE_ENABLE"),
        {memory->getInputPinDynamic("DMEM_WRITE_ENABLE"), data_op->getInputPinDynamic("IN_0")});
    w.connect(coordinator->getOutputPinDynamic("PIPELINE_STALL"),
        {getOutputPinDynamic("PIPELINE_STALL")});
    for (const auto* signal : {
             "LOAD_USE_STALL", "MEMORY_STALL",
             "DATA_PORT_STALL", "PIPELINE_FLUSH"}) {
        w.connect(coordinator->getOutputPinDynamic(signal),
            {getOutputPinDynamic(signal)});
    }

    for (const auto* name : {"HALT_EVENT", "TRAP_EVENT"})
        w.connect(writeback_stage_->getOutputPinDynamic(name),
            {execution_status_->getInputPinDynamic(name)});
    w.connect(writeback_stage_->getOutputPinDynamic("TRAP_CAUSE"),
        {execution_status_->getInputPinDynamic("EVENT_TRAP_CAUSE")});
    w.connect(execution_status_->getOutputPinDynamic("TRAP_CAUSE"),
        {getOutputPinDynamic("TRAP_CAUSE")});
    w.connect(writeback_stage_->getOutputPinDynamic("COMMIT_VALID"),
        {getOutputPinDynamic("COMMIT_VALID"), getOutputPinDynamic("INSTRUCTION_ATTEMPT")});
    w.connect(memory->getOutputPinDynamic("DMEM_ADDR"),
        {lanes->getInputPinDynamic("ADDR"), data_request->getInputPinDynamic("TAG")});
    w.connect(memory->getOutputPinDynamic("DMEM_WRITE_DATA"),
        {lanes->getInputPinDynamic("WRITE_DATA")});
    w.connect(memory->getOutputPinDynamic("DMEM_SIZE"), {lanes->getInputPinDynamic("SIZE")});
    w.connect(memory->getOutputPinDynamic("DMEM_SIGN_EXTEND"),
        {lanes->getInputPinDynamic("SIGN_EXTEND")});
    w.finish();
}

rv32i::RV32IArchitecturalState
RV32IFiveStageCore::snapshotPipelineState(uint32_t observed_pc) const {
    if (!decode_stage_ || !writeback_stage_) {
        throw std::logic_error("RV32IFiveStageCore is not built");
    }
    const auto register_view =
        std::dynamic_pointer_cast<RegisterStateView>(decode_stage_);
    if (!register_view) {
        throw std::logic_error(
            "Selected decode stage lacks register-state observation");
    }

    rv32i::RV32IArchitecturalState state;
    state.pc = rv32i::RV32IArchitecturalState::fromKnown(rv32i::RV32IState{.pc=observed_pc}).pc;
    const auto registers = register_view->getRegisterStateAtTime(
        std::numeric_limits<size_t>::max());
    for (size_t index = 0;
         index < state.x.size() && index < registers.size();
         ++index) {
        state.x[index] = registers[index];
    }
    state.halted =
        execution_status_->getOutputPin("HALTED")->getValue();
    state.trapped =
        execution_status_->getOutputPin("TRAPPED")->getValue();
    state.trap_cause =
        execution_status_->getOutputPin<4>("TRAP_CAUSE")
            ->getValueAsVector();
    return state;
}

RV32IRetirementObservation RV32IFiveStageCore::pendingRetirement() const {
    return rv32i::observeRetirementPacket(*retirement_packet_, writeback_stage_->getOutputPin("TERMINAL")->getValue()==LogicValue::HIGH);
}
