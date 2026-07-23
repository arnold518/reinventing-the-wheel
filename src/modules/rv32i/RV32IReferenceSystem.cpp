#include "modules/rv32i/RV32IReferenceSystem.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include <stdexcept>
#include <utility>

namespace circuit::families {
const ComponentFamily RV32IReferenceSystem{
    "rv32i.system.oracle-backed-reference",
    "RV32IReferenceSystem",
    nullptr,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IReferenceSystem>(
            context, name);
    }};
}

BEGIN_PINS(RV32IReferenceSystem, IOComponent)
    INPUT_PIN("CLK")
    INPUT_PIN("RST")
    INPUT_PIN("ENABLE")
    OUTPUT_PIN_WIDTH("PC", 32)
    OUTPUT_PIN("HALTED")
    OUTPUT_PIN("TRAPPED")
END_PINS()

void RV32IReferenceSystem::buildInternals(ComponentBuilder& builder) {
    core_ = std::dynamic_pointer_cast<RV32IReferenceCore>(
        builder.add(circuit::families::RV32IReferenceCore, "CORE"));
    instruction_memory_ = std::dynamic_pointer_cast<Memory64Kx32>(
        builder.add(circuit::families::Memory64Kx32, "INSTRUCTION_MEMORY"));
    data_memory_ = std::dynamic_pointer_cast<Memory64Kx32>(
        builder.add(circuit::families::Memory64Kx32, "DATA_MEMORY"));
    core_->attachMemories(instruction_memory_, data_memory_);

    builder.addNewWire("CLK_internal", getInputPin("CLK"), {builder.getInputPin<RV32IReferenceCore>("CORE", "CLK")});
    builder.addNewWire("RST_internal", getInputPin("RST"), {builder.getInputPin<RV32IReferenceCore>("CORE", "RST")});
    builder.addNewWire("ENABLE_internal", getInputPin("ENABLE"), {builder.getInputPin<RV32IReferenceCore>("CORE", "ENABLE")});

    builder.addNewWire<32>(
        "PC_internal",
        builder.getOutputPin<RV32IReferenceCore, 32>("CORE", "PC"),
        {getOutputPin<32>("PC")});
    builder.addNewWire(
        "HALTED_internal",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "HALTED"),
        {getOutputPin("HALTED")});
    builder.addNewWire(
        "TRAPPED_internal",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "TRAPPED"),
        {getOutputPin("TRAPPED")});

    builder.addNewWire<32>(
        "IMEM_ADDR",
        builder.getOutputPin<RV32IReferenceCore, 32>("CORE", "IMEM_ADDR"),
        {builder.getInputPin<Memory64Kx32, 32>("INSTRUCTION_MEMORY", "ADDR")});
    builder.addNewWire<32>(
        "IMEM_WRITE_DATA",
        builder.getOutputPin<RV32IReferenceCore, 32>("CORE", "IMEM_WRITE_DATA"),
        {builder.getInputPin<Memory64Kx32, 32>("INSTRUCTION_MEMORY", "WRITE_DATA")});
    builder.addNewWire(
        "IMEM_READ_EN",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "IMEM_READ_EN"),
        {builder.getInputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "READ_EN")});
    builder.addNewWire(
        "IMEM_WRITE_EN",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "IMEM_WRITE_EN"),
        {builder.getInputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "WRITE_EN")});
    builder.addNewWire<2>(
        "IMEM_SIZE",
        builder.getOutputPin<RV32IReferenceCore, 2>("CORE", "IMEM_SIZE"),
        {builder.getInputPin<Memory64Kx32, 2>("INSTRUCTION_MEMORY", "SIZE")});
    builder.addNewWire(
        "IMEM_SIGN_EXTEND",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "IMEM_SIGN_EXTEND"),
        {builder.getInputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "SIGN_EXTEND")});
    builder.addNewWire(
        "IMEM_CLK",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "IMEM_CLK"),
        {builder.getInputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "CLK")});
    builder.addNewWire(
        "IMEM_RST",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "IMEM_RST"),
        {builder.getInputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "RST")});

    builder.addNewWire<32>(
        "IMEM_READ_DATA",
        builder.getOutputPin<Memory64Kx32, 32>("INSTRUCTION_MEMORY", "READ_DATA"),
        {builder.getInputPin<RV32IReferenceCore, 32>("CORE", "IMEM_READ_DATA")});
    builder.addNewWire(
        "IMEM_READY",
        builder.getOutputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "READY"),
        {builder.getInputPin<RV32IReferenceCore>("CORE", "IMEM_READY")});
    builder.addNewWire(
        "IMEM_FAULT",
        builder.getOutputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "FAULT"),
        {builder.getInputPin<RV32IReferenceCore>("CORE", "IMEM_FAULT")});

    builder.addNewWire<32>(
        "DMEM_ADDR",
        builder.getOutputPin<RV32IReferenceCore, 32>("CORE", "DMEM_ADDR"),
        {builder.getInputPin<Memory64Kx32, 32>("DATA_MEMORY", "ADDR")});
    builder.addNewWire<32>(
        "DMEM_WRITE_DATA",
        builder.getOutputPin<RV32IReferenceCore, 32>("CORE", "DMEM_WRITE_DATA"),
        {builder.getInputPin<Memory64Kx32, 32>("DATA_MEMORY", "WRITE_DATA")});
    builder.addNewWire(
        "DMEM_READ_EN",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "DMEM_READ_EN"),
        {builder.getInputPin<Memory64Kx32>("DATA_MEMORY", "READ_EN")});
    builder.addNewWire(
        "DMEM_WRITE_EN",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "DMEM_WRITE_EN"),
        {builder.getInputPin<Memory64Kx32>("DATA_MEMORY", "WRITE_EN")});
    builder.addNewWire<2>(
        "DMEM_SIZE",
        builder.getOutputPin<RV32IReferenceCore, 2>("CORE", "DMEM_SIZE"),
        {builder.getInputPin<Memory64Kx32, 2>("DATA_MEMORY", "SIZE")});
    builder.addNewWire(
        "DMEM_SIGN_EXTEND",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "DMEM_SIGN_EXTEND"),
        {builder.getInputPin<Memory64Kx32>("DATA_MEMORY", "SIGN_EXTEND")});
    builder.addNewWire(
        "DMEM_CLK",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "DMEM_CLK"),
        {builder.getInputPin<Memory64Kx32>("DATA_MEMORY", "CLK")});
    builder.addNewWire(
        "DMEM_RST",
        builder.getOutputPin<RV32IReferenceCore>("CORE", "DMEM_RST"),
        {builder.getInputPin<Memory64Kx32>("DATA_MEMORY", "RST")});

    builder.addNewWire<32>(
        "DMEM_READ_DATA",
        builder.getOutputPin<Memory64Kx32, 32>("DATA_MEMORY", "READ_DATA"),
        {builder.getInputPin<RV32IReferenceCore, 32>("CORE", "DMEM_READ_DATA")});
    builder.addNewWire(
        "DMEM_READY",
        builder.getOutputPin<Memory64Kx32>("DATA_MEMORY", "READY"),
        {builder.getInputPin<RV32IReferenceCore>("CORE", "DMEM_READY")});
    builder.addNewWire(
        "DMEM_FAULT",
        builder.getOutputPin<Memory64Kx32>("DATA_MEMORY", "FAULT"),
        {builder.getInputPin<RV32IReferenceCore>("CORE", "DMEM_FAULT")});
}

void RV32IReferenceSystem::setInitialPC(uint32_t pc) {
    if (!core_) {
        throw std::logic_error("RV32IReferenceSystem core is not built");
    }
    core_->setInitialPC(pc);
}

void RV32IReferenceSystem::setRegister(uint8_t index, uint32_t value) {
    if (!core_) {
        throw std::logic_error("RV32IReferenceSystem core is not built");
    }
    core_->setRegister(index, value);
}

void RV32IReferenceSystem::resetCore() {
    if (!core_) {
        throw std::logic_error("RV32IReferenceSystem core is not built");
    }
    core_->resetCore();
}

void RV32IReferenceSystem::clearInstructionMemory() {
    if (!instruction_memory_) {
        throw std::logic_error("RV32IReferenceSystem instruction memory is not built");
    }
    instruction_memory_->clearContents();
}

void RV32IReferenceSystem::clearDataMemory() {
    if (!data_memory_) {
        throw std::logic_error("RV32IReferenceSystem data memory is not built");
    }
    data_memory_->clearContents();
}

void RV32IReferenceSystem::loadProgram(const rv32i::RV32IProgram& program, uint32_t base_address) {
    if (!instruction_memory_) {
        throw std::logic_error("RV32IReferenceSystem instruction memory is not built");
    }
    program.loadInto(*instruction_memory_, base_address);
}

void RV32IReferenceSystem::loadInstructionBytes(uint32_t base_address, const std::vector<uint8_t>& data) {
    if (!instruction_memory_) {
        throw std::logic_error("RV32IReferenceSystem instruction memory is not built");
    }
    instruction_memory_->loadBytes(base_address, data);
}

void RV32IReferenceSystem::loadDataBytes(uint32_t base_address, const std::vector<uint8_t>& data) {
    if (!data_memory_) {
        throw std::logic_error("RV32IReferenceSystem data memory is not built");
    }
    data_memory_->loadBytes(base_address, data);
}

void RV32IReferenceSystem::loadDataWords(uint32_t base_address, const std::vector<uint32_t>& words) {
    if (!data_memory_) {
        throw std::logic_error("RV32IReferenceSystem data memory is not built");
    }
    data_memory_->loadWords(base_address, words);
}

rv32i::RV32IState RV32IReferenceSystem::snapshotState() const {
    if (!core_) {
        throw std::logic_error("RV32IReferenceSystem core is not built");
    }
    return core_->snapshotState();
}

rv32i::RV32IMemoryTrace RV32IReferenceSystem::lastDataMemoryAccess() const {
    if (!core_) {
        throw std::logic_error("RV32IReferenceSystem core is not built");
    }
    return core_->lastDataMemoryAccess();
}

std::map<uint32_t, uint8_t> RV32IReferenceSystem::lastDataMemoryWrites() const {
    if (!core_) {
        throw std::logic_error("RV32IReferenceSystem core is not built");
    }
    return core_->lastDataMemoryWrites();
}
