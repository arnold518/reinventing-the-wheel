#include "modules/rv32i/RV32ISingleCycleSystem.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/rv32i/RV32IReferenceSystem.hpp"
#include "modules/utility/Constant.hpp"
#include <stdexcept>
#include <utility>

namespace {
void initializeSystemPins(IOComponent* self) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("ENABLE", PinType::INPUT);
    self->addPin<32>("PC", PinType::OUTPUT);
    self->addPin("HALTED", PinType::OUTPUT);
    self->addPin("TRAPPED", PinType::OUTPUT);
}
} // namespace

namespace circuit::families {
const ComponentFamily RV32ISingleCycleSystem{
    "rv32i.system.educational-single-cycle",
    "RV32ISingleCycleSystem",
    initializeSystemPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32ISingleCycleSystem>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IReferenceSystem>(
            context, name);
    }};
}

RV32ISingleCycleSystem::RV32ISingleCycleSystem(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32ISingleCycleSystem.pinInitializer()) {}

void RV32ISingleCycleSystem::buildInternals(ComponentBuilder& builder) {
    core_ = builder.add(circuit::families::RV32ISingleCycleCore, "CORE");
    instruction_memory_ = std::dynamic_pointer_cast<Memory64Kx32>(
        builder.add(circuit::families::Memory64Kx32, "INSTRUCTION_MEMORY"));
    data_memory_ = std::dynamic_pointer_cast<Memory64Kx32>(
        builder.add(circuit::families::Memory64Kx32, "DATA_MEMORY"));
    builder.addNewComponent<ConstantValue<1>>("CONST_LOW", 0);
    builder.addNewComponent<ConstantValue<2>>("CONST_WORD_SIZE", 2);
    builder.addNewComponent<ConstantValue<32>>("CONST_ZERO32", 0);

    builder.addNewWire(
        "CLK_fanout",
        getInputPin("CLK"),
        {builder.getInputPin<RV32ISingleCycleCore>("CORE", "CLK"),
         builder.getInputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "CLK"),
         builder.getInputPin<Memory64Kx32>("DATA_MEMORY", "CLK")});
    builder.addNewWire(
        "RST_to_core",
        getInputPin("RST"),
        {builder.getInputPin<RV32ISingleCycleCore>("CORE", "RST")});
    builder.addNewWire(
        "ENABLE_to_core",
        getInputPin("ENABLE"),
        {builder.getInputPin<RV32ISingleCycleCore>("CORE", "ENABLE")});

    builder.addNewWire<32>(
        "PC_to_output",
        builder.getOutputPin<RV32ISingleCycleCore, 32>("CORE", "PC"),
        {getOutputPin<32>("PC")});
    builder.addNewWire(
        "HALTED_to_output",
        builder.getOutputPin<RV32ISingleCycleCore>("CORE", "HALTED"),
        {getOutputPin("HALTED")});
    builder.addNewWire(
        "TRAPPED_to_output",
        builder.getOutputPin<RV32ISingleCycleCore>("CORE", "TRAPPED"),
        {getOutputPin("TRAPPED")});

    builder.addNewWire<32>(
        "IMEM_ADDR",
        builder.getOutputPin<RV32ISingleCycleCore, 32>("CORE", "IMEM_ADDR"),
        {builder.getInputPin<Memory64Kx32, 32>("INSTRUCTION_MEMORY", "ADDR")});
    builder.addNewWire(
        "IMEM_READ_EN",
        builder.getOutputPin<RV32ISingleCycleCore>("CORE", "IMEM_READ_EN"),
        {builder.getInputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "READ_EN")});
    builder.addNewWire<32>(
        "IMEM_READ_DATA",
        builder.getOutputPin<Memory64Kx32, 32>("INSTRUCTION_MEMORY", "READ_DATA"),
        {builder.getInputPin<RV32ISingleCycleCore, 32>("CORE", "IMEM_READ_DATA")});
    builder.addNewWire(
        "IMEM_READY",
        builder.getOutputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "READY"),
        {builder.getInputPin<RV32ISingleCycleCore>("CORE", "IMEM_READY")});
    builder.addNewWire(
        "IMEM_FAULT",
        builder.getOutputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "FAULT"),
        {builder.getInputPin<RV32ISingleCycleCore>("CORE", "IMEM_FAULT")});

    builder.addNewWire<32>(
        "DMEM_ADDR",
        builder.getOutputPin<RV32ISingleCycleCore, 32>("CORE", "DMEM_ADDR"),
        {builder.getInputPin<Memory64Kx32, 32>("DATA_MEMORY", "ADDR")});
    builder.addNewWire<32>(
        "DMEM_WRITE_DATA",
        builder.getOutputPin<RV32ISingleCycleCore, 32>("CORE", "DMEM_WRITE_DATA"),
        {builder.getInputPin<Memory64Kx32, 32>("DATA_MEMORY", "WRITE_DATA")});
    builder.addNewWire(
        "DMEM_READ_EN",
        builder.getOutputPin<RV32ISingleCycleCore>("CORE", "DMEM_READ_EN"),
        {builder.getInputPin<Memory64Kx32>("DATA_MEMORY", "READ_EN")});
    builder.addNewWire(
        "DMEM_WRITE_EN",
        builder.getOutputPin<RV32ISingleCycleCore>("CORE", "DMEM_WRITE_EN"),
        {builder.getInputPin<Memory64Kx32>("DATA_MEMORY", "WRITE_EN")});
    builder.addNewWire<2>(
        "DMEM_SIZE",
        builder.getOutputPin<RV32ISingleCycleCore, 2>("CORE", "DMEM_SIZE"),
        {builder.getInputPin<Memory64Kx32, 2>("DATA_MEMORY", "SIZE")});
    builder.addNewWire(
        "DMEM_SIGN_EXTEND",
        builder.getOutputPin<RV32ISingleCycleCore>("CORE", "DMEM_SIGN_EXTEND"),
        {builder.getInputPin<Memory64Kx32>("DATA_MEMORY", "SIGN_EXTEND")});
    builder.addNewWire<32>(
        "DMEM_READ_DATA",
        builder.getOutputPin<Memory64Kx32, 32>("DATA_MEMORY", "READ_DATA"),
        {builder.getInputPin<RV32ISingleCycleCore, 32>("CORE", "DMEM_READ_DATA")});
    builder.addNewWire(
        "DMEM_READY",
        builder.getOutputPin<Memory64Kx32>("DATA_MEMORY", "READY"),
        {builder.getInputPin<RV32ISingleCycleCore>("CORE", "DMEM_READY")});
    builder.addNewWire(
        "DMEM_FAULT",
        builder.getOutputPin<Memory64Kx32>("DATA_MEMORY", "FAULT"),
        {builder.getInputPin<RV32ISingleCycleCore>("CORE", "DMEM_FAULT")});

    builder.addNewWire(
        "CONST_LOW_fanout",
        builder.getOutputPin<ConstantValue<1>>("CONST_LOW", "OUT"),
        {builder.getInputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "WRITE_EN"),
         builder.getInputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "SIGN_EXTEND"),
         builder.getInputPin<Memory64Kx32>("INSTRUCTION_MEMORY", "RST"),
         builder.getInputPin<Memory64Kx32>("DATA_MEMORY", "RST")});
    builder.addNewWire<2>(
        "CONST_WORD_SIZE_to_imem",
        builder.getOutputPin<ConstantValue<2>, 2>("CONST_WORD_SIZE", "OUT"),
        {builder.getInputPin<Memory64Kx32, 2>("INSTRUCTION_MEMORY", "SIZE")});
    builder.addNewWire<32>(
        "CONST_ZERO32_to_imem",
        builder.getOutputPin<ConstantValue<32>, 32>("CONST_ZERO32", "OUT"),
        {builder.getInputPin<Memory64Kx32, 32>("INSTRUCTION_MEMORY", "WRITE_DATA")});
}

void RV32ISingleCycleSystem::clearInstructionMemory() {
    if (!instruction_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    instruction_memory_->clearContents();
}

void RV32ISingleCycleSystem::clearDataMemory() {
    if (!data_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    data_memory_->clearContents();
}

void RV32ISingleCycleSystem::loadProgram(const rv32i::RV32IProgram& program, uint32_t base_address) {
    if (!instruction_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    program.loadInto(*instruction_memory_, base_address);
}

void RV32ISingleCycleSystem::loadInstructionBytes(uint32_t base_address, const std::vector<uint8_t>& data) {
    if (!instruction_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    instruction_memory_->loadBytes(base_address, data);
}

void RV32ISingleCycleSystem::loadDataBytes(uint32_t base_address, const std::vector<uint8_t>& data) {
    if (!data_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    data_memory_->loadBytes(base_address, data);
}

std::vector<uint8_t> RV32ISingleCycleSystem::readDataBytes(
    uint32_t base_address,
    size_t count) const {
    if (!data_memory_) {
        throw std::logic_error("RV32ISingleCycleSystem is not built");
    }
    return data_memory_->readBytes(base_address, count);
}

void RV32ISingleCycleSystem::setProgramMemoryHistoryRecordingEnabled(
    bool enabled) {
    if (!instruction_memory_ || !data_memory_) {
        throw std::logic_error("RV32ISingleCycleSystem is not built");
    }
    instruction_memory_->setHistoryRecordingEnabled(enabled);
    data_memory_->setHistoryRecordingEnabled(enabled);
}

void RV32ISingleCycleSystem::loadDataWords(uint32_t base_address, const std::vector<uint32_t>& words) {
    if (!data_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    data_memory_->loadWords(base_address, words);
}

rv32i::RV32IArchitecturalState
RV32ISingleCycleSystem::snapshotArchitecturalState() const {
    if (!core_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    const auto state_view = std::dynamic_pointer_cast<RV32IStateView>(core_);
    if (!state_view) {
        throw std::logic_error(
            "Selected RV32I core lacks architectural-state observation");
    }
    return state_view->snapshotArchitecturalState();
}

rv32i::RV32IMemoryTrace
RV32ISingleCycleSystem::lastCommittedDataMemoryAccess() const {
    // The structural program harness captures bus intent immediately before
    // the active edge. It is the authoritative observation for this fidelity.
    return {};
}

std::map<uint32_t, uint8_t>
RV32ISingleCycleSystem::dataMemoryWritesInTimeRange(
    size_t start_time,
    size_t end_time) const {
    if (!data_memory_) {
        throw std::logic_error(
            "RV32ISingleCycleSystem is not built");
    }
    return data_memory_->getByteWritesInTimeRange(
        start_time, end_time);
}
