#include "modules/rv32i/RV32IReferenceSystem.hpp"

#include "components/Component.hpp"
#include "modules/rv32i/RV32ISingleCycleSystem.hpp"
#include "rv32i/RV32IInstructionOracle.hpp"
#include <stdexcept>
#include <utility>

namespace {
LogicValue sanitizeBit(LogicValue value) {
    return value == LogicValue::HIGH || value == LogicValue::LOW
        ? value
        : LogicValue::UNKNOWN;
}

size_t memorySizeBytes(rv32i::RV32IMemorySize size) {
    switch (size) {
        case rv32i::RV32IMemorySize::None: return 0;
        case rv32i::RV32IMemorySize::Byte: return 1;
        case rv32i::RV32IMemorySize::Halfword: return 2;
        case rv32i::RV32IMemorySize::Word: return 4;
    }
    return 0;
}
} // namespace

RV32IReferenceSystem::RV32IReferenceSystem(std::string name)
    : BasicComponent(
          name,
          1,
          circuit::families::RV32ISingleCycleSystem.pinInitializer()),
      instruction_memory_(
          Component::create<Memory64Kx32>(
              name + ".INSTRUCTION_MEMORY_STATE")),
      data_memory_(
          Component::create<Memory64Kx32>(
              name + ".DATA_MEMORY_STATE")) {}

void RV32IReferenceSystem::setInitialPC(uint32_t pc) {
    reset_pc_ = pc;
    state_.pc = pc;
}

void RV32IReferenceSystem::setRegister(
    uint8_t index,
    uint32_t value) {
    state_.writeRegister(index, value);
}

void RV32IReferenceSystem::resetCore() {
    state_ = rv32i::RV32IState{};
    state_.pc = reset_pc_;
    state_.forceX0();
    last_data_memory_access_ = {};
    last_data_memory_writes_.clear();
}

void RV32IReferenceSystem::clearInstructionMemory() {
    instruction_memory_->clearContents();
}

void RV32IReferenceSystem::clearDataMemory() {
    data_memory_->clearContents();
    data_memory_write_history_.clear();
}

void RV32IReferenceSystem::loadProgram(
    const rv32i::RV32IProgram& program,
    uint32_t base_address) {
    program.loadInto(*instruction_memory_, base_address);
}

void RV32IReferenceSystem::loadInstructionBytes(
    uint32_t base_address,
    const std::vector<uint8_t>& data) {
    instruction_memory_->loadBytes(base_address, data);
}

void RV32IReferenceSystem::loadDataBytes(
    uint32_t base_address,
    const std::vector<uint8_t>& data) {
    data_memory_->loadBytes(base_address, data);
}

void RV32IReferenceSystem::loadDataWords(
    uint32_t base_address,
    const std::vector<uint32_t>& words) {
    data_memory_->loadWords(base_address, words);
}

std::map<uint32_t, uint8_t>
RV32IReferenceSystem::dataMemoryWritesInTimeRange(
    size_t start_time,
    size_t end_time) const {
    if (end_time < start_time) {
        throw std::invalid_argument(
            "RV32I behavioral-system write-history range runs backward");
    }
    std::map<uint32_t, uint8_t> result;
    for (const auto& write : data_memory_write_history_) {
        if (write.time <= start_time || write.time > end_time) {
            continue;
        }
        result[write.address] = write.value;
    }
    return result;
}

void RV32IReferenceSystem::applyDataMemoryWrite(
    size_t current_time) {
    last_data_memory_writes_.clear();
    const auto& memory = last_data_memory_access_;
    if (memory.kind != rv32i::RV32IMemoryAccessKind::Write
        || memory.fault) {
        return;
    }
    const auto byte_count = memorySizeBytes(memory.size);
    for (size_t byte = 0; byte < byte_count; ++byte) {
        const auto address =
            static_cast<uint32_t>(memory.address + byte);
        const auto value = static_cast<uint8_t>(
            (memory.write_data >> (byte * 8)) & 0xffU);
        data_memory_->writeU8AtTime(
            current_time,
            address,
            value);
        last_data_memory_writes_[address] = value;
        data_memory_write_history_.push_back({
            current_time,
            address,
            value,
        });
    }
}

void RV32IReferenceSystem::evaluate(
    size_t current_time,
    Simulator& simulator) {
    const auto clk = sanitizeBit(getInputValue("CLK"));
    const auto rst = sanitizeBit(getInputValue("RST"));
    const auto enable = sanitizeBit(getInputValue("ENABLE"));
    const bool active_step =
        previous_clk_ == LogicValue::LOW
        && clk == LogicValue::HIGH
        && enable == LogicValue::HIGH;

    if (rst == LogicValue::HIGH) {
        resetCore();
    } else if (active_step && !state_.halted && !state_.trapped) {
        const auto trace =
            rv32i::RV32IInstructionOracle::stepWithoutDataMemoryWrite(
                state_,
                *instruction_memory_,
                *data_memory_);
        last_data_memory_access_ = trace.memory;
        applyDataMemoryWrite(current_time);
    }

    previous_clk_ = clk;
    publishOutputs(simulator, current_time);
}

void RV32IReferenceSystem::publishOutputs(
    Simulator& simulator,
    size_t current_time) {
    _updateOutputWire<32>(
        simulator,
        "PC",
        state_.pc,
        current_time);
    _updateOutputWire(
        simulator,
        "HALTED",
        state_.halted ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire(
        simulator,
        "TRAPPED",
        state_.trapped ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
}
