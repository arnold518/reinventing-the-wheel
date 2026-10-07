#include "modules/rv32i/RV32IPipelineStages.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "RV32IPipelineStagesInternal.hpp"

namespace {
void fetchStagePins(IOComponent* self) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("PC_WRITE", PinType::INPUT);
    self->addPin("PC_REDIRECT", PinType::INPUT);
    self->addPin("FETCH_VALID", PinType::INPUT);
    self->addPin<32>("REDIRECT_PC", PinType::INPUT);
    self->addPin<32>("IMEM_READ_DATA", PinType::INPUT);
    self->addPin("IMEM_FAULT", PinType::INPUT);

    self->addPin<32>("FETCH_PC", PinType::OUTPUT);
    self->addPin<32>("IMEM_ADDR", PinType::OUTPUT);
    self->addPin("OUT_VALID", PinType::OUTPUT);
    self->addPin<32>("OUT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_INSTRUCTION", PinType::OUTPUT);
    self->addPin<32>("OUT_FETCH_STATUS", PinType::OUTPUT);
}

void decodeStagePins(IOComponent* self) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("IN_VALID", PinType::INPUT);
    self->addPin<32>("IN_PC", PinType::INPUT);
    self->addPin<32>("IN_INSTRUCTION", PinType::INPUT);
    self->addPin<32>("IN_FETCH_STATUS", PinType::INPUT);
    self->addPin<32>("EX_CONTROL", PinType::INPUT);
    self->addPin<32>("EX_REGISTER_ADDRESSES", PinType::INPUT);
    self->addPin("EX_VALID", PinType::INPUT);
    self->addPin("WB_VALID", PinType::INPUT);
    self->addPin("WB_REG_WRITE", PinType::INPUT);
    self->addPin("WB_REGISTER_WRITE", PinType::INPUT);
    self->addPin<5>("WB_RD", PinType::INPUT);
    self->addPin<32>("WB_VALUE", PinType::INPUT);

    self->addPin("OUT_VALID", PinType::OUTPUT);
    self->addPin<32>("OUT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_INSTRUCTION", PinType::OUTPUT);
    self->addPin<32>("OUT_RS1_VALUE", PinType::OUTPUT);
    self->addPin<32>("OUT_RS2_VALUE", PinType::OUTPUT);
    self->addPin<32>("OUT_IMMEDIATE", PinType::OUTPUT);
    self->addPin<32>("OUT_CONTROL", PinType::OUTPUT);
    self->addPin<32>("OUT_REGISTER_ADDRESSES", PinType::OUTPUT);
    self->addPin<32>("OUT_FETCH_STATUS", PinType::OUTPUT);
    self->addPin("LOAD_USE_HAZARD", PinType::OUTPUT);
    self->addPin("TERMINAL", PinType::OUTPUT);
}

void executeStagePins(IOComponent* self) {
    self->addPin("IN_VALID", PinType::INPUT);
    self->addPin<32>("IN_PC", PinType::INPUT);
    self->addPin<32>("IN_INSTRUCTION", PinType::INPUT);
    self->addPin<32>("IN_RS1_VALUE", PinType::INPUT);
    self->addPin<32>("IN_RS2_VALUE", PinType::INPUT);
    self->addPin<32>("IN_IMMEDIATE", PinType::INPUT);
    self->addPin<32>("IN_CONTROL", PinType::INPUT);
    self->addPin<32>("IN_REGISTER_ADDRESSES", PinType::INPUT);
    self->addPin<32>("IN_FETCH_STATUS", PinType::INPUT);
    self->addPin("EX_MEM_VALID", PinType::INPUT);
    self->addPin<5>("EX_MEM_RD", PinType::INPUT);
    self->addPin("EX_MEM_REG_WRITE", PinType::INPUT);
    self->addPin("EX_MEM_RESULT_READY", PinType::INPUT);
    self->addPin<32>("EX_MEM_VALUE", PinType::INPUT);
    self->addPin("MEM_WB_VALID", PinType::INPUT);
    self->addPin<5>("MEM_WB_RD", PinType::INPUT);
    self->addPin("MEM_WB_REG_WRITE", PinType::INPUT);
    self->addPin<32>("MEM_WB_VALUE", PinType::INPUT);

    self->addPin("OUT_VALID", PinType::OUTPUT);
    self->addPin<32>("OUT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_INSTRUCTION", PinType::OUTPUT);
    self->addPin<32>("OUT_ALU_RESULT", PinType::OUTPUT);
    self->addPin<32>("OUT_STORE_DATA", PinType::OUTPUT);
    self->addPin<32>("OUT_PC_PLUS_4", PinType::OUTPUT);
    self->addPin<32>("OUT_NEXT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_CONTROL", PinType::OUTPUT);
    self->addPin<32>("OUT_REGISTER_ADDRESSES", PinType::OUTPUT);
    self->addPin<32>("OUT_FETCH_STATUS", PinType::OUTPUT);
    self->addPin<32>("OUT_EXECUTION_STATUS", PinType::OUTPUT);
    self->addPin("REDIRECT", PinType::OUTPUT);
    self->addPin<32>("REDIRECT_PC", PinType::OUTPUT);
    self->addPin("PRETERMINAL", PinType::OUTPUT);
    self->addPin("IS_BRANCH", PinType::OUTPUT);
    self->addPin("BRANCH_TAKEN", PinType::OUTPUT);
    self->addPin("CONTROL_TAKEN", PinType::OUTPUT);
    self->addPin<32>("CONTROL_TARGET", PinType::OUTPUT);
}

void memoryStagePins(IOComponent* self) {
    self->addPin("IN_VALID", PinType::INPUT);
    self->addPin<32>("IN_PC", PinType::INPUT);
    self->addPin<32>("IN_INSTRUCTION", PinType::INPUT);
    self->addPin<32>("IN_ALU_RESULT", PinType::INPUT);
    self->addPin<32>("IN_STORE_DATA", PinType::INPUT);
    self->addPin<32>("IN_PC_PLUS_4", PinType::INPUT);
    self->addPin<32>("IN_NEXT_PC", PinType::INPUT);
    self->addPin<32>("IN_CONTROL", PinType::INPUT);
    self->addPin<32>("IN_REGISTER_ADDRESSES", PinType::INPUT);
    self->addPin<32>("IN_FETCH_STATUS", PinType::INPUT);
    self->addPin<32>("IN_EXECUTION_STATUS", PinType::INPUT);
    self->addPin<32>("DMEM_READ_DATA", PinType::INPUT);
    self->addPin("DMEM_FAULT", PinType::INPUT);
    self->addPin("DMEM_READ_ENABLE", PinType::INPUT);
    self->addPin("DMEM_WRITE_ENABLE", PinType::INPUT);
    self->addPin("OUT_VALID", PinType::OUTPUT);
    self->addPin<32>("OUT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_INSTRUCTION", PinType::OUTPUT);
    self->addPin<32>("OUT_ALU_RESULT", PinType::OUTPUT);
    self->addPin<32>("OUT_MEMORY_DATA", PinType::OUTPUT);
    self->addPin<32>("OUT_STORE_DATA", PinType::OUTPUT);
    self->addPin<32>("OUT_PC_PLUS_4", PinType::OUTPUT);
    self->addPin<32>("OUT_NEXT_PC", PinType::OUTPUT);
    self->addPin<32>("OUT_CONTROL", PinType::OUTPUT);
    self->addPin<32>("OUT_REGISTER_ADDRESSES", PinType::OUTPUT);
    self->addPin<32>("OUT_FETCH_STATUS", PinType::OUTPUT);
    self->addPin<32>("OUT_EXECUTION_STATUS", PinType::OUTPUT);
    self->addPin<32>("OUT_MEMORY_STATUS", PinType::OUTPUT);

    self->addPin("LOAD_REQUEST", PinType::OUTPUT);
    self->addPin("STORE_REQUEST", PinType::OUTPUT);
    self->addPin("LOAD_FAULT", PinType::OUTPUT);
    self->addPin("STORE_FAULT", PinType::OUTPUT);
    self->addPin("MEMORY_FAULT", PinType::OUTPUT);
    self->addPin("PRETERMINAL", PinType::OUTPUT);
    self->addPin<5>("FORWARD_RD", PinType::OUTPUT);
    self->addPin("FORWARD_REG_WRITE", PinType::OUTPUT);
    self->addPin("FORWARD_RESULT_READY", PinType::OUTPUT);
    self->addPin<32>("FORWARD_VALUE", PinType::OUTPUT);

    self->addPin<32>("DMEM_ADDR", PinType::OUTPUT);
    self->addPin<32>("DMEM_WRITE_DATA", PinType::OUTPUT);
    self->addPin<2>("DMEM_SIZE", PinType::OUTPUT);
    self->addPin("DMEM_SIGN_EXTEND", PinType::OUTPUT);
}

void writebackStagePins(IOComponent* self) {
    self->addPin("COMMIT_ENABLE", PinType::INPUT);
    self->addPin("IN_VALID", PinType::INPUT);
    self->addPin<32>("IN_ALU_RESULT", PinType::INPUT);
    self->addPin<32>("IN_MEMORY_DATA", PinType::INPUT);
    self->addPin<32>("IN_PC_PLUS_4", PinType::INPUT);
    self->addPin<32>("IN_CONTROL", PinType::INPUT);
    self->addPin<32>("IN_REGISTER_ADDRESSES", PinType::INPUT);
    self->addPin<32>("IN_FETCH_STATUS", PinType::INPUT);
    self->addPin<32>("IN_EXECUTION_STATUS", PinType::INPUT);
    self->addPin<32>("IN_MEMORY_STATUS", PinType::INPUT);

    self->addPin("HALT_EVENT", PinType::OUTPUT);
    self->addPin("TRAP_EVENT", PinType::OUTPUT);
    self->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
    self->addPin("COMMIT_VALID", PinType::OUTPUT);

    self->addPin("WB_VALID", PinType::OUTPUT);
    self->addPin("WB_REG_WRITE", PinType::OUTPUT);
    self->addPin("REGISTER_WRITE", PinType::OUTPUT);
    self->addPin<5>("WB_RD", PinType::OUTPUT);
    self->addPin<32>("WB_VALUE", PinType::OUTPUT);
    self->addPin("TERMINAL", PinType::OUTPUT);
    self->addPin("RETIRE_TERMINAL", PinType::OUTPUT);
}

void executionStatusPins(IOComponent* self) {
    for (auto name : {"CLK", "RST", "HALT_EVENT", "TRAP_EVENT"})
        self->addPin(name, PinType::INPUT);
    self->addPin<4>("EVENT_TRAP_CAUSE", PinType::INPUT);
    self->addPin("HALTED", PinType::OUTPUT);
    self->addPin("TRAPPED", PinType::OUTPUT);
    self->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
}
} // namespace

namespace circuit::families {
bool externalFetchPC(const std::shared_ptr<BuildContext>& context) {
    if (!context || context->parameters().empty()) return false;
    const auto& parameters = context->parameters();
    if (parameters.size() != 1 || !parameters.contains("external_next_pc") ||
        (parameters.at("external_next_pc") != "0" && parameters.at("external_next_pc") != "1"))
        throw std::invalid_argument("RV32IFetchStage external_next_pc must be 0 or 1");
    return parameters.at("external_next_pc") == "1";
}
const ComponentFamily RV32IFetchStage{
    "rv32i.pipeline.stage.fetch",
    "RV32IFetchStage",
    fetchStagePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IFetchStage>(
            context, name, externalFetchPC(context));
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return rv32i::pipeline_stages::directFetchStage(name, context, externalFetchPC(context));
    }};

const ComponentFamily RV32IDecodeStage{
    "rv32i.pipeline.stage.decode",
    "RV32IDecodeStage",
    decodeStagePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IDecodeStage>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return rv32i::pipeline_stages::directDecodeStage(name, context);
    }};

const ComponentFamily RV32IExecuteStage{
    "rv32i.pipeline.stage.execute",
    "RV32IExecuteStage",
    executeStagePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IExecuteStage>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return rv32i::pipeline_stages::directExecuteStage(name, context);
    }};

const ComponentFamily RV32IMemoryStage{
    "rv32i.pipeline.stage.memory",
    "RV32IMemoryStage",
    memoryStagePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IMemoryStage>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return rv32i::pipeline_stages::directMemoryStage(name, context);
    }};

const ComponentFamily RV32IPipelineExecutionStatus{
    "rv32i.pipeline.execution-status", "RV32IPipelineExecutionStatus", executionStatusPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IPipelineExecutionStatus>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return rv32i::pipeline_stages::directExecutionStatus(name, context);
    }};
const ComponentFamily RV32IWritebackStage{
    "rv32i.pipeline.stage.writeback",
    "RV32IWritebackStage",
    writebackStagePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IWritebackStage>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return rv32i::pipeline_stages::directWritebackStage(name, context);
    }};
} // namespace circuit::families
