#include "modules/rv32i/RV32IPipelineControl.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "RV32IPipelineControlInternal.hpp"

namespace {
void forwardingPins(IOComponent* self) {
    self->addPin<5>("ID_EX_RS1", PinType::INPUT);
    self->addPin<5>("ID_EX_RS2", PinType::INPUT);
    self->addPin<5>("EX_MEM_RD", PinType::INPUT);
    self->addPin("EX_MEM_VALID", PinType::INPUT);
    self->addPin("EX_MEM_REG_WRITE", PinType::INPUT);
    self->addPin("EX_MEM_RESULT_READY", PinType::INPUT);
    self->addPin<5>("MEM_WB_RD", PinType::INPUT);
    self->addPin("MEM_WB_VALID", PinType::INPUT);
    self->addPin("MEM_WB_REG_WRITE", PinType::INPUT);
    self->addPin<2>("FORWARD_A", PinType::OUTPUT);
    self->addPin<2>("FORWARD_B", PinType::OUTPUT);
}

void hazardPins(IOComponent* self) {
    self->addPin("ID_VALID", PinType::INPUT);
    self->addPin<5>("ID_RS1", PinType::INPUT);
    self->addPin<5>("ID_RS2", PinType::INPUT);
    self->addPin("ID_USES_RS1", PinType::INPUT);
    self->addPin("ID_USES_RS2", PinType::INPUT);
    self->addPin("EX_VALID", PinType::INPUT);
    self->addPin("EX_MEM_READ", PinType::INPUT);
    self->addPin<5>("EX_RD", PinType::INPUT);
    self->addPin("LOAD_USE_HAZARD", PinType::OUTPUT);
    self->addPin("STALL_FETCH", PinType::OUTPUT);
    self->addPin("STALL_IF_ID", PinType::OUTPUT);
    self->addPin("BUBBLE_ID_EX", PinType::OUTPUT);
}

void memoryAlignmentPins(IOComponent* self) {
    self->addPin<32>("ADDRESS", PinType::INPUT);
    self->addPin<2>("SIZE", PinType::INPUT);
    self->addPin("MISALIGNED", PinType::OUTPUT);
}

void pipelineRetirementPins(IOComponent* self) {
    self->addPin("ENABLE", PinType::INPUT);
    self->addPin("MEM_WB_VALID", PinType::INPUT);
    self->addPin("PC_MISALIGNED", PinType::INPUT);
    self->addPin("INSTRUCTION_FAULT", PinType::INPUT);
    self->addPin("LEGAL", PinType::INPUT);
    self->addPin("TRAP_REQUEST", PinType::INPUT);
    self->addPin<4>("DECODE_TRAP_CAUSE", PinType::INPUT);
    self->addPin("MEM_READ", PinType::INPUT);
    self->addPin("MEM_WRITE", PinType::INPUT);
    self->addPin("DATA_MISALIGNED", PinType::INPUT);
    self->addPin("DATA_FAULT", PinType::INPUT);
    self->addPin("TARGET_MISALIGNED", PinType::INPUT);
    self->addPin("HALT_REQUEST", PinType::INPUT);
    self->addPin("REG_WRITE_REQUEST", PinType::INPUT);

    self->addPin("COMMIT", PinType::OUTPUT);
    self->addPin("NORMAL_COMMIT", PinType::OUTPUT);
    self->addPin("REGISTER_WRITE", PinType::OUTPUT);
    self->addPin("HALT_EVENT", PinType::OUTPUT);
    self->addPin("TRAP_EVENT", PinType::OUTPUT);
    self->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
}

void pipelineCoordinatorPins(IOComponent* self) {
    self->addPin("ENABLE", PinType::INPUT);
    self->addPin("HALTED", PinType::INPUT);
    self->addPin("TRAPPED", PinType::INPUT);
    self->addPin("IMEM_READY", PinType::INPUT);
    self->addPin("DMEM_READY", PinType::INPUT);
    self->addPin("LOAD_USE_HAZARD", PinType::INPUT);
    self->addPin("TERMINAL_PENDING", PinType::INPUT);
    self->addPin("EX_PRETERMINAL", PinType::INPUT);
    self->addPin("MEMORY_TERMINAL", PinType::INPUT);
    self->addPin("WB_TERMINAL", PinType::INPUT);
    self->addPin("EX_REDIRECT", PinType::INPUT);
    self->addPin("MEMORY_FAULT", PinType::INPUT);
    self->addPin("MEM_LOAD", PinType::INPUT);
    self->addPin("MEM_STORE", PinType::INPUT);
    self->addPin("RETIRE_TERMINAL", PinType::INPUT);

    self->addPin("ACTIVE", PinType::OUTPUT);
    self->addPin("FETCH_RESPONSE_TAKE", PinType::OUTPUT);
    self->addPin("DATA_REQUEST_ISSUE", PinType::OUTPUT);
    self->addPin("IMEM_READ_ENABLE", PinType::OUTPUT);
    self->addPin("DMEM_READ_ENABLE", PinType::OUTPUT);
    self->addPin("DMEM_WRITE_ENABLE", PinType::OUTPUT);
    self->addPin("FETCH_VALID", PinType::OUTPUT);
    self->addPin("FETCH_PC_WRITE", PinType::OUTPUT);
    self->addPin("FETCH_PC_REDIRECT", PinType::OUTPUT);
    self->addPin("IF_ID_WRITE", PinType::OUTPUT);
    self->addPin("IF_ID_FLUSH", PinType::OUTPUT);
    self->addPin("ID_EX_WRITE", PinType::OUTPUT);
    self->addPin("ID_EX_FLUSH", PinType::OUTPUT);
    self->addPin("EX_MEM_WRITE", PinType::OUTPUT);
    self->addPin("EX_MEM_FLUSH", PinType::OUTPUT);
    self->addPin("MEM_WB_WRITE", PinType::OUTPUT);
    self->addPin("MEM_WB_FLUSH", PinType::OUTPUT);
    self->addPin("COMMIT_ENABLE", PinType::OUTPUT);
    self->addPin("PIPELINE_STALL", PinType::OUTPUT);
    self->addPin("LOAD_USE_STALL", PinType::OUTPUT);
    self->addPin("MEMORY_STALL", PinType::OUTPUT);
    self->addPin("DATA_PORT_STALL", PinType::OUTPUT);
    self->addPin("PIPELINE_FLUSH", PinType::OUTPUT);
}
} // namespace

namespace circuit::families {
const ComponentFamily RV32IForwardingUnit{
    "rv32i.pipeline.forwarding",
    "RV32IForwardingUnit",
    forwardingPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IForwardingUnit>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return rv32i::pipeline_control::directForwardingUnit(name, context);
    }};

const ComponentFamily RV32IHazardDetectionUnit{
    "rv32i.pipeline.hazard-detection",
    "RV32IHazardDetectionUnit",
    hazardPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IHazardDetectionUnit>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return rv32i::pipeline_control::directHazardDetectionUnit(name, context);
    }};

const ComponentFamily RV32IMemoryAlignmentUnit{
    "rv32i.pipeline.memory-alignment",
    "RV32IMemoryAlignmentUnit",
    memoryAlignmentPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IMemoryAlignmentUnit>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return rv32i::pipeline_control::directMemoryAlignmentUnit(name, context);
    }};

const ComponentFamily RV32IPipelineRetirementUnit{
    "rv32i.pipeline.retirement",
    "RV32IPipelineRetirementUnit",
    pipelineRetirementPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IPipelineRetirementUnit>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return rv32i::pipeline_control::directPipelineRetirementUnit(name, context);
    }};

const ComponentFamily RV32IPipelineCoordinator{
    "rv32i.pipeline.coordinator",
    "RV32IPipelineCoordinator",
    pipelineCoordinatorPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IPipelineCoordinator>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return rv32i::pipeline_control::directPipelineCoordinator(name, context);
    }};
} // namespace circuit::families
