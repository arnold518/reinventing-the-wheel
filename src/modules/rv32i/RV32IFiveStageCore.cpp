#include "modules/rv32i/RV32IFiveStageCore.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "modules/rv32i/RV32IFiveStageCoreDirect.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPipelineRegisters.hpp"
#include "modules/rv32i/RV32IPipelineStages.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
void fiveStageCorePins(IOComponent* self) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("ENABLE", PinType::INPUT);
    self->addPin<32>("IMEM_READ_DATA", PinType::INPUT);
    self->addPin("IMEM_READY", PinType::INPUT);
    self->addPin("IMEM_FAULT", PinType::INPUT);
    self->addPin<32>("DMEM_READ_DATA", PinType::INPUT);
    self->addPin("DMEM_READY", PinType::INPUT);
    self->addPin("DMEM_FAULT", PinType::INPUT);

    self->addPin<32>("PC", PinType::OUTPUT);
    self->addPin<32>("FETCH_PC", PinType::OUTPUT);
    self->addPin("HALTED", PinType::OUTPUT);
    self->addPin("TRAPPED", PinType::OUTPUT);
    self->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
    self->addPin("INSTRUCTION_ATTEMPT", PinType::OUTPUT);
    self->addPin("COMMIT_VALID", PinType::OUTPUT);
    self->addPin<32>("RETIRED_COUNT", PinType::OUTPUT);
    self->addPin<32>("RETIRED_PC", PinType::OUTPUT);
    self->addPin<32>("RETIRED_INSTRUCTION", PinType::OUTPUT);
    self->addPin("RETIRED_MEM_READ", PinType::OUTPUT);
    self->addPin("RETIRED_MEM_WRITE", PinType::OUTPUT);
    self->addPin<32>("RETIRED_MEM_ADDR", PinType::OUTPUT);
    self->addPin<32>("RETIRED_MEM_WRITE_DATA", PinType::OUTPUT);
    self->addPin<32>("RETIRED_MEM_READ_DATA", PinType::OUTPUT);
    self->addPin<2>("RETIRED_MEM_SIZE", PinType::OUTPUT);
    self->addPin("RETIRED_MEM_SIGN_EXTEND", PinType::OUTPUT);
    self->addPin("RETIRED_MEM_FAULT", PinType::OUTPUT);

    self->addPin<32>("IMEM_ADDR", PinType::OUTPUT);
    self->addPin("IMEM_READ_EN", PinType::OUTPUT);
    self->addPin<32>("DMEM_ADDR", PinType::OUTPUT);
    self->addPin<32>("DMEM_WRITE_DATA", PinType::OUTPUT);
    self->addPin("DMEM_READ_EN", PinType::OUTPUT);
    self->addPin("DMEM_WRITE_EN", PinType::OUTPUT);
    self->addPin<2>("DMEM_SIZE", PinType::OUTPUT);
    self->addPin("DMEM_SIGN_EXTEND", PinType::OUTPUT);

    self->addPin("PIPELINE_STALL", PinType::OUTPUT);
    self->addPin("LOAD_USE_STALL", PinType::OUTPUT);
    self->addPin("MEMORY_STALL", PinType::OUTPUT);
    self->addPin("DATA_PORT_STALL", PinType::OUTPUT);
    self->addPin("PIPELINE_FLUSH", PinType::OUTPUT);
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
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IFiveStageCoreDirect>(context, name);
    }};
} // namespace circuit::families

RV32IFiveStageCore::RV32IFiveStageCore(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IFiveStageCore.pinInitializer()) {}

void RV32IFiveStageCore::buildInternals(ComponentBuilder& builder) {
    const auto fetch = builder.add(
        circuit::families::RV32IFetchStage, "FETCH");
    const auto if_id = builder.add(
        circuit::families::RV32IIFIDPipelineRegister, "IF_ID");
    decode_stage_ = builder.add(
        circuit::families::RV32IDecodeStage, "DECODE");
    const auto id_ex = builder.add(
        circuit::families::RV32IIDEXPipelineRegister, "ID_EX");
    const auto execute = builder.add(
        circuit::families::RV32IExecuteStage, "EXECUTE");
    const auto ex_mem = builder.add(
        circuit::families::RV32IEXMEMPipelineRegister, "EX_MEM");
    const auto memory = builder.add(
        circuit::families::RV32IMemoryStage, "MEMORY");
    const auto mem_wb = builder.add(
        circuit::families::RV32IMEMWBPipelineRegister, "MEM_WB");
    writeback_stage_ = builder.add(
        circuit::families::RV32IWritebackStage, "WRITEBACK");
    const auto coordinator = builder.add(
        circuit::families::RV32IPipelineCoordinator, "COORDINATOR");

    builder.addNewWire(
        "CLK",
        getInputPin("CLK"),
        {fetch->getInputPin("CLK"),
         if_id->getInputPin("CLK"),
         decode_stage_->getInputPin("CLK"),
         id_ex->getInputPin("CLK"),
         ex_mem->getInputPin("CLK"),
         mem_wb->getInputPin("CLK"),
         writeback_stage_->getInputPin("CLK")});
    builder.addNewWire(
        "RST",
        getInputPin("RST"),
        {fetch->getInputPin("RST"),
         if_id->getInputPin("RST"),
         decode_stage_->getInputPin("RST"),
         id_ex->getInputPin("RST"),
         ex_mem->getInputPin("RST"),
         mem_wb->getInputPin("RST"),
         writeback_stage_->getInputPin("RST")});
    builder.addNewWire(
        "ENABLE",
        getInputPin("ENABLE"),
        {coordinator->getInputPin("ENABLE")});
    builder.addNewWire<32>(
        "IMEM_READ_DATA",
        getInputPin<32>("IMEM_READ_DATA"),
        {fetch->getInputPin<32>("IMEM_READ_DATA")});
    builder.addNewWire(
        "IMEM_READY",
        getInputPin("IMEM_READY"),
        {coordinator->getInputPin("IMEM_READY")});
    builder.addNewWire(
        "IMEM_FAULT",
        getInputPin("IMEM_FAULT"),
        {fetch->getInputPin("IMEM_FAULT")});
    builder.addNewWire<32>(
        "DMEM_READ_DATA",
        getInputPin<32>("DMEM_READ_DATA"),
        {memory->getInputPin<32>("DMEM_READ_DATA")});
    builder.addNewWire(
        "DMEM_READY",
        getInputPin("DMEM_READY"),
        {coordinator->getInputPin("DMEM_READY")});
    builder.addNewWire(
        "DMEM_FAULT",
        getInputPin("DMEM_FAULT"),
        {memory->getInputPin("DMEM_FAULT")});

    builder.addNewWire<32>(
        "FETCH_PC",
        fetch->getOutputPin<32>("FETCH_PC"),
        {getOutputPin<32>("FETCH_PC")});
    builder.addNewWire<32>(
        "IMEM_ADDR",
        fetch->getOutputPin<32>("IMEM_ADDR"),
        {getOutputPin<32>("IMEM_ADDR")});
    builder.addNewWire(
        "FETCH_OUT_VALID",
        fetch->getOutputPin("OUT_VALID"),
        {if_id->getInputPin("D_VALID")});
    for (const auto* field : {
             "PC", "INSTRUCTION", "FETCH_STATUS"}) {
        builder.addNewWire<32>(
            "FETCH_OUT_" + std::string(field),
            fetch->getOutputPin<32>(
                "OUT_" + std::string(field)),
            {if_id->getInputPin<32>(
                "D_" + std::string(field))});
    }

    builder.addNewWire(
        "IF_ID_VALID",
        if_id->getOutputPin("Q_VALID"),
        {decode_stage_->getInputPin("IN_VALID"),
         getOutputPin("IF_ID_VALID")});
    builder.addNewWire<32>(
        "IF_ID_PC",
        if_id->getOutputPin<32>("Q_PC"),
        {decode_stage_->getInputPin<32>("IN_PC"),
         getOutputPin<32>("IF_ID_PC")});
    builder.addNewWire<32>(
        "IF_ID_INSTRUCTION",
        if_id->getOutputPin<32>("Q_INSTRUCTION"),
        {decode_stage_->getInputPin<32>("IN_INSTRUCTION")});
    builder.addNewWire<32>(
        "IF_ID_FETCH_STATUS",
        if_id->getOutputPin<32>("Q_FETCH_STATUS"),
        {decode_stage_->getInputPin<32>("IN_FETCH_STATUS")});

    builder.addNewWire(
        "DECODE_OUT_VALID",
        decode_stage_->getOutputPin("OUT_VALID"),
        {id_ex->getInputPin("D_VALID")});
    for (const auto* field : {
             "PC", "INSTRUCTION", "RS1_VALUE", "RS2_VALUE",
             "IMMEDIATE", "CONTROL", "REGISTER_ADDRESSES",
             "FETCH_STATUS"}) {
        builder.addNewWire<32>(
            "DECODE_OUT_" + std::string(field),
            decode_stage_->getOutputPin<32>(
                "OUT_" + std::string(field)),
            {id_ex->getInputPin<32>(
                "D_" + std::string(field))});
    }

    builder.addNewWire(
        "ID_EX_VALID",
        id_ex->getOutputPin("Q_VALID"),
        {execute->getInputPin("IN_VALID"),
         decode_stage_->getInputPin("EX_VALID"),
         getOutputPin("ID_EX_VALID")});
    builder.addNewWire<32>(
        "ID_EX_PC",
        id_ex->getOutputPin<32>("Q_PC"),
        {execute->getInputPin<32>("IN_PC"),
         getOutputPin<32>("ID_EX_PC")});
    for (const auto* field : {
             "INSTRUCTION", "RS1_VALUE", "RS2_VALUE", "IMMEDIATE",
             "FETCH_STATUS"}) {
        builder.addNewWire<32>(
            "ID_EX_" + std::string(field),
            id_ex->getOutputPin<32>(
                "Q_" + std::string(field)),
            {execute->getInputPin<32>(
                "IN_" + std::string(field))});
    }
    builder.addNewWire<32>(
        "ID_EX_CONTROL",
        id_ex->getOutputPin<32>("Q_CONTROL"),
        {execute->getInputPin<32>("IN_CONTROL"),
         decode_stage_->getInputPin<32>("EX_CONTROL")});
    builder.addNewWire<32>(
        "ID_EX_ADDRESSES",
        id_ex->getOutputPin<32>("Q_REGISTER_ADDRESSES"),
        {execute->getInputPin<32>("IN_REGISTER_ADDRESSES"),
         decode_stage_->getInputPin<32>(
             "EX_REGISTER_ADDRESSES")});

    builder.addNewWire(
        "EXECUTE_OUT_VALID",
        execute->getOutputPin("OUT_VALID"),
        {ex_mem->getInputPin("D_VALID")});
    for (const auto* field : {
             "PC", "INSTRUCTION", "ALU_RESULT", "STORE_DATA",
             "PC_PLUS_4", "NEXT_PC", "CONTROL",
             "REGISTER_ADDRESSES", "FETCH_STATUS",
             "EXECUTION_STATUS"}) {
        builder.addNewWire<32>(
            "EXECUTE_OUT_" + std::string(field),
            execute->getOutputPin<32>(
                "OUT_" + std::string(field)),
            {ex_mem->getInputPin<32>(
                "D_" + std::string(field))});
    }
    builder.addNewWire(
        "EXECUTE_REDIRECT",
        execute->getOutputPin("REDIRECT"),
        {coordinator->getInputPin("EX_REDIRECT")});
    builder.addNewWire<32>(
        "EXECUTE_REDIRECT_PC",
        execute->getOutputPin<32>("REDIRECT_PC"),
        {fetch->getInputPin<32>("REDIRECT_PC")});
    builder.addNewWire(
        "EXECUTE_PRETERMINAL",
        execute->getOutputPin("PRETERMINAL"),
        {coordinator->getInputPin("EX_PRETERMINAL")});

    builder.addNewWire(
        "EX_MEM_VALID",
        ex_mem->getOutputPin("Q_VALID"),
        {memory->getInputPin("IN_VALID"),
         execute->getInputPin("EX_MEM_VALID"),
         getOutputPin("EX_MEM_VALID")});
    builder.addNewWire<32>(
        "EX_MEM_PC",
        ex_mem->getOutputPin<32>("Q_PC"),
        {memory->getInputPin<32>("IN_PC"),
         getOutputPin<32>("EX_MEM_PC")});
    for (const auto* field : {
             "INSTRUCTION", "ALU_RESULT", "STORE_DATA", "PC_PLUS_4",
             "NEXT_PC", "CONTROL", "REGISTER_ADDRESSES",
             "FETCH_STATUS", "EXECUTION_STATUS"}) {
        builder.addNewWire<32>(
            "EX_MEM_" + std::string(field),
            ex_mem->getOutputPin<32>(
                "Q_" + std::string(field)),
            {memory->getInputPin<32>(
                "IN_" + std::string(field))});
    }

    builder.addNewWire(
        "MEMORY_OUT_VALID",
        memory->getOutputPin("OUT_VALID"),
        {mem_wb->getInputPin("D_VALID")});
    for (const auto* field : {
             "PC", "INSTRUCTION", "ALU_RESULT", "MEMORY_DATA",
             "STORE_DATA", "PC_PLUS_4", "NEXT_PC", "CONTROL",
             "REGISTER_ADDRESSES", "FETCH_STATUS",
             "EXECUTION_STATUS", "MEMORY_STATUS"}) {
        builder.addNewWire<32>(
            "MEMORY_OUT_" + std::string(field),
            memory->getOutputPin<32>(
                "OUT_" + std::string(field)),
            {mem_wb->getInputPin<32>(
                "D_" + std::string(field))});
    }
    builder.addNewWire(
        "MEMORY_LOAD_REQUEST",
        memory->getOutputPin("LOAD_REQUEST"),
        {coordinator->getInputPin("MEM_LOAD")});
    builder.addNewWire(
        "MEMORY_STORE_REQUEST",
        memory->getOutputPin("STORE_REQUEST"),
        {coordinator->getInputPin("MEM_STORE")});
    builder.addNewWire(
        "MEMORY_FAULT",
        memory->getOutputPin("MEMORY_FAULT"),
        {coordinator->getInputPin("MEMORY_FAULT")});
    builder.addNewWire(
        "MEMORY_PRETERMINAL",
        memory->getOutputPin("PRETERMINAL"),
        {coordinator->getInputPin("MEMORY_TERMINAL")});
    builder.addNewWire<5>(
        "MEMORY_FORWARD_RD",
        memory->getOutputPin<5>("FORWARD_RD"),
        {execute->getInputPin<5>("EX_MEM_RD")});
    builder.addNewWire(
        "MEMORY_FORWARD_REG_WRITE",
        memory->getOutputPin("FORWARD_REG_WRITE"),
        {execute->getInputPin("EX_MEM_REG_WRITE")});
    builder.addNewWire(
        "MEMORY_FORWARD_READY",
        memory->getOutputPin("FORWARD_RESULT_READY"),
        {execute->getInputPin("EX_MEM_RESULT_READY")});
    builder.addNewWire<32>(
        "MEMORY_FORWARD_VALUE",
        memory->getOutputPin<32>("FORWARD_VALUE"),
        {execute->getInputPin<32>("EX_MEM_VALUE")});

    builder.addNewWire(
        "MEM_WB_VALID",
        mem_wb->getOutputPin("Q_VALID"),
        {writeback_stage_->getInputPin("IN_VALID"),
         getOutputPin("MEM_WB_VALID")});
    builder.addNewWire<32>(
        "MEM_WB_PC",
        mem_wb->getOutputPin<32>("Q_PC"),
        {writeback_stage_->getInputPin<32>("IN_PC"),
         getOutputPin<32>("MEM_WB_PC")});
    for (const auto* field : {
             "INSTRUCTION", "ALU_RESULT", "MEMORY_DATA", "STORE_DATA",
             "PC_PLUS_4", "NEXT_PC", "CONTROL",
             "REGISTER_ADDRESSES", "FETCH_STATUS",
             "EXECUTION_STATUS", "MEMORY_STATUS"}) {
        builder.addNewWire<32>(
            "MEM_WB_" + std::string(field),
            mem_wb->getOutputPin<32>(
                "Q_" + std::string(field)),
            {writeback_stage_->getInputPin<32>(
                "IN_" + std::string(field))});
    }

    builder.addNewWire(
        "WB_VALID",
        writeback_stage_->getOutputPin("WB_VALID"),
        {decode_stage_->getInputPin("WB_VALID"),
         execute->getInputPin("MEM_WB_VALID")});
    builder.addNewWire(
        "WB_REG_WRITE",
        writeback_stage_->getOutputPin("WB_REG_WRITE"),
        {decode_stage_->getInputPin("WB_REG_WRITE"),
         execute->getInputPin("MEM_WB_REG_WRITE")});
    builder.addNewWire(
        "WB_REGISTER_WRITE",
        writeback_stage_->getOutputPin("REGISTER_WRITE"),
        {decode_stage_->getInputPin("WB_REGISTER_WRITE")});
    builder.addNewWire<5>(
        "WB_RD",
        writeback_stage_->getOutputPin<5>("WB_RD"),
        {decode_stage_->getInputPin<5>("WB_RD"),
         execute->getInputPin<5>("MEM_WB_RD")});
    builder.addNewWire<32>(
        "WB_VALUE",
        writeback_stage_->getOutputPin<32>("WB_VALUE"),
        {decode_stage_->getInputPin<32>("WB_VALUE"),
         execute->getInputPin<32>("MEM_WB_VALUE")});
    builder.addNewWire(
        "WB_TERMINAL",
        writeback_stage_->getOutputPin("TERMINAL"),
        {coordinator->getInputPin("WB_TERMINAL")});
    builder.addNewWire(
        "RETIRE_TERMINAL",
        writeback_stage_->getOutputPin("RETIRE_TERMINAL"),
        {coordinator->getInputPin("RETIRE_TERMINAL")});
    builder.addNewWire(
        "HALTED",
        writeback_stage_->getOutputPin("HALTED"),
        {coordinator->getInputPin("HALTED"),
         getOutputPin("HALTED")});
    builder.addNewWire(
        "TRAPPED",
        writeback_stage_->getOutputPin("TRAPPED"),
        {coordinator->getInputPin("TRAPPED"),
         getOutputPin("TRAPPED")});

    builder.addNewWire(
        "DECODE_TERMINAL",
        decode_stage_->getOutputPin("TERMINAL"),
        {coordinator->getInputPin("TERMINAL_PENDING")});
    builder.addNewWire(
        "LOAD_USE_HAZARD",
        decode_stage_->getOutputPin("LOAD_USE_HAZARD"),
        {coordinator->getInputPin("LOAD_USE_HAZARD")});

    builder.addNewWire(
        "COORDINATOR_FETCH_VALID",
        coordinator->getOutputPin("FETCH_VALID"),
        {fetch->getInputPin("FETCH_VALID")});
    builder.addNewWire(
        "COORDINATOR_FETCH_PC_WRITE",
        coordinator->getOutputPin("FETCH_PC_WRITE"),
        {fetch->getInputPin("PC_WRITE")});
    builder.addNewWire(
        "COORDINATOR_FETCH_REDIRECT",
        coordinator->getOutputPin("FETCH_PC_REDIRECT"),
        {fetch->getInputPin("PC_REDIRECT")});
    builder.addNewWire(
        "COORDINATOR_IF_ID_WRITE",
        coordinator->getOutputPin("IF_ID_WRITE"),
        {if_id->getInputPin("WRITE_ENABLE")});
    builder.addNewWire(
        "COORDINATOR_IF_ID_FLUSH",
        coordinator->getOutputPin("IF_ID_FLUSH"),
        {if_id->getInputPin("FLUSH")});
    builder.addNewWire(
        "COORDINATOR_ID_EX_WRITE",
        coordinator->getOutputPin("ID_EX_WRITE"),
        {id_ex->getInputPin("WRITE_ENABLE")});
    builder.addNewWire(
        "COORDINATOR_ID_EX_FLUSH",
        coordinator->getOutputPin("ID_EX_FLUSH"),
        {id_ex->getInputPin("FLUSH")});
    builder.addNewWire(
        "COORDINATOR_EX_MEM_WRITE",
        coordinator->getOutputPin("EX_MEM_WRITE"),
        {ex_mem->getInputPin("WRITE_ENABLE")});
    builder.addNewWire(
        "COORDINATOR_EX_MEM_FLUSH",
        coordinator->getOutputPin("EX_MEM_FLUSH"),
        {ex_mem->getInputPin("FLUSH")});
    builder.addNewWire(
        "COORDINATOR_MEM_WB_WRITE",
        coordinator->getOutputPin("MEM_WB_WRITE"),
        {mem_wb->getInputPin("WRITE_ENABLE")});
    builder.addNewWire(
        "COORDINATOR_MEM_WB_FLUSH",
        coordinator->getOutputPin("MEM_WB_FLUSH"),
        {mem_wb->getInputPin("FLUSH")});
    builder.addNewWire(
        "COORDINATOR_COMMIT_ENABLE",
        coordinator->getOutputPin("COMMIT_ENABLE"),
        {writeback_stage_->getInputPin("COMMIT_ENABLE")});
    builder.addNewWire(
        "COORDINATOR_IMEM_READ",
        coordinator->getOutputPin("IMEM_READ_ENABLE"),
        {getOutputPin("IMEM_READ_EN")});
    builder.addNewWire(
        "COORDINATOR_DMEM_READ",
        coordinator->getOutputPin("DMEM_READ_ENABLE"),
        {memory->getInputPin("DMEM_READ_ENABLE"),
         getOutputPin("DMEM_READ_EN")});
    builder.addNewWire(
        "COORDINATOR_DMEM_WRITE",
        coordinator->getOutputPin("DMEM_WRITE_ENABLE"),
        {memory->getInputPin("DMEM_WRITE_ENABLE"),
         getOutputPin("DMEM_WRITE_EN")});
    builder.addNewWire(
        "COORDINATOR_STALL",
        coordinator->getOutputPin("PIPELINE_STALL"),
        {getOutputPin("PIPELINE_STALL")});
    for (const auto* signal : {
             "LOAD_USE_STALL", "MEMORY_STALL",
             "DATA_PORT_STALL", "PIPELINE_FLUSH"}) {
        builder.addNewWire(
            std::string("COORDINATOR_") + signal,
            coordinator->getOutputPin(signal),
            {getOutputPin(signal)});
    }

    builder.addNewWire<32>(
        "PC",
        writeback_stage_->getOutputPin<32>("PC"),
        {getOutputPin<32>("PC")});
    builder.addNewWire<4>(
        "TRAP_CAUSE",
        writeback_stage_->getOutputPin<4>("TRAP_CAUSE"),
        {getOutputPin<4>("TRAP_CAUSE")});
    builder.addNewWire(
        "COMMIT_VALID",
        writeback_stage_->getOutputPin("COMMIT_VALID"),
        {getOutputPin("COMMIT_VALID"),
         getOutputPin("INSTRUCTION_ATTEMPT")});
    for (const auto* field : {
             "RETIRED_COUNT", "RETIRED_PC", "RETIRED_INSTRUCTION",
             "RETIRED_MEM_ADDR", "RETIRED_MEM_WRITE_DATA",
             "RETIRED_MEM_READ_DATA"}) {
        builder.addNewWire<32>(
            field,
            writeback_stage_->getOutputPin<32>(field),
            {getOutputPin<32>(field)});
    }
    for (const auto* field : {
             "RETIRED_MEM_READ", "RETIRED_MEM_WRITE",
             "RETIRED_MEM_SIGN_EXTEND", "RETIRED_MEM_FAULT"}) {
        builder.addNewWire(
            field,
            writeback_stage_->getOutputPin(field),
            {getOutputPin(field)});
    }
    builder.addNewWire<2>(
        "RETIRED_MEM_SIZE",
        writeback_stage_->getOutputPin<2>("RETIRED_MEM_SIZE"),
        {getOutputPin<2>("RETIRED_MEM_SIZE")});
    builder.addNewWire<32>(
        "DMEM_ADDR",
        memory->getOutputPin<32>("DMEM_ADDR"),
        {getOutputPin<32>("DMEM_ADDR")});
    builder.addNewWire<32>(
        "DMEM_WRITE_DATA",
        memory->getOutputPin<32>("DMEM_WRITE_DATA"),
        {getOutputPin<32>("DMEM_WRITE_DATA")});
    builder.addNewWire<2>(
        "DMEM_SIZE",
        memory->getOutputPin<2>("DMEM_SIZE"),
        {getOutputPin<2>("DMEM_SIZE")});
    builder.addNewWire(
        "DMEM_SIGN_EXTEND",
        memory->getOutputPin("DMEM_SIGN_EXTEND"),
        {getOutputPin("DMEM_SIGN_EXTEND")});
}

rv32i::RV32IArchitecturalState
RV32IFiveStageCore::snapshotArchitecturalState() const {
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
    state.pc =
        writeback_stage_->getOutputPin<32>("PC")->getValueAsVector();
    const auto registers = register_view->getRegisterStateAtTime(
        std::numeric_limits<size_t>::max());
    for (size_t index = 0;
         index < state.x.size() && index < registers.size();
         ++index) {
        state.x[index] = registers[index];
    }
    state.halted =
        writeback_stage_->getOutputPin("HALTED")->getValue();
    state.trapped =
        writeback_stage_->getOutputPin("TRAPPED")->getValue();
    state.trap_cause =
        writeback_stage_->getOutputPin<4>("TRAP_CAUSE")
            ->getValueAsVector();
    return state;
}
