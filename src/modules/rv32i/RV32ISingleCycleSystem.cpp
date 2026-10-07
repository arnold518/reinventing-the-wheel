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

RV32ISingleCycleSystem::RV32ISingleCycleSystem(std::string name,
    rv32i::MemoryConfiguration instructions, rv32i::MemoryConfiguration data)
    : IOComponent(
          std::move(name),
          circuit::families::RV32ISingleCycleSystem.pinInitializer()),
      instruction_config_(std::move(instructions)), data_config_(std::move(data)) {}

void RV32ISingleCycleSystem::buildInternals(ComponentBuilder& builder) {
    core_ = builder.add(circuit::families::RV32ISingleCycleCore, "CORE");
    if(instruction_config_.technology!=circuit::memory::Technology::FF || data_config_.technology!=circuit::memory::Technology::FF)
        throw std::invalid_argument("Strict single-cycle core requires zero-wait FF endpoints");
    instruction_memory_ = rv32i::buildMemory(builder, *this, "INSTRUCTION_MEMORY", instruction_config_);
    data_memory_ = rv32i::buildMemory(builder, *this, "DATA_MEMORY", data_config_);
    rv32i::wireMemorySystem(builder, core_, instruction_memory_, data_memory_,
        getInputPin("CLK"), getInputPin("RST"));
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

}

void RV32ISingleCycleSystem::clearInstructionMemory() {
    if (!instruction_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    rv32i::memoryView(instruction_memory_)->clearMemoryImage();
}

void RV32ISingleCycleSystem::clearDataMemory() {
    if (!data_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    rv32i::memoryView(data_memory_)->clearMemoryImage();
}

void RV32ISingleCycleSystem::loadProgram(const rv32i::RV32IProgram& program, uint32_t base_address) {
    if (!instruction_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    rv32i::memoryView(instruction_memory_)->initializeMemoryBytes(base_address, program.bytes());
}

void RV32ISingleCycleSystem::loadInstructionBytes(uint32_t base_address, const std::vector<uint8_t>& data) {
    if (!instruction_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    rv32i::memoryView(instruction_memory_)->initializeMemoryBytes(base_address, data);
}

void RV32ISingleCycleSystem::loadDataBytes(uint32_t base_address, const std::vector<uint8_t>& data) {
    if (!data_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    rv32i::memoryView(data_memory_)->initializeMemoryBytes(base_address, data);
}

std::vector<uint8_t> RV32ISingleCycleSystem::readDataBytes(
    uint32_t base_address,
    size_t count) const {
    if (!data_memory_) {
        throw std::logic_error("RV32ISingleCycleSystem is not built");
    }
    return rv32i::memoryView(data_memory_)->memoryStorage().readBytes(base_address, count);
}

void RV32ISingleCycleSystem::setProgramMemoryHistoryRecordingEnabled(
    bool enabled) {
    if (!instruction_memory_ || !data_memory_) {
        throw std::logic_error("RV32ISingleCycleSystem is not built");
    }
    rv32i::memoryView(instruction_memory_)->setMemoryHistoryRecordingEnabled(enabled);
    rv32i::memoryView(data_memory_)->setMemoryHistoryRecordingEnabled(enabled);
}

void RV32ISingleCycleSystem::loadDataWords(uint32_t base_address, const std::vector<uint32_t>& words) {
    if (!data_memory_) throw std::logic_error("RV32ISingleCycleSystem is not built");
    loadDataBytes(base_address, rv32i::RV32IProgram::fromWords(words).bytes());
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
    return rv32i::memoryView(data_memory_)->memoryStorage().getByteWritesInTimeRange(
        start_time, end_time);
}
