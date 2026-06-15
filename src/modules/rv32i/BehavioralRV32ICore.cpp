#include "modules/rv32i/BehavioralRV32ICore.hpp"

#include "basic/Pin.hpp"
#include "rv32i/RV32IInstructionOracle.hpp"
#include "simulator/Simulator.hpp"
#include <utility>

namespace {
LogicValue sanitizeBit(LogicValue value) {
    return (value == LogicValue::HIGH || value == LogicValue::LOW) ? value : LogicValue::UNKNOWN;
}

uint64_t encodeMemorySize(rv32i::RV32IMemorySize size) {
    switch (size) {
        case rv32i::RV32IMemorySize::Byte: return 0;
        case rv32i::RV32IMemorySize::Halfword: return 1;
        case rv32i::RV32IMemorySize::Word: return 2;
        case rv32i::RV32IMemorySize::None: return 2;
    }
    return 2;
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

std::map<uint32_t, uint8_t> expandDataMemoryWrites(const rv32i::RV32IMemoryTrace& memory) {
    std::map<uint32_t, uint8_t> writes;
    if (memory.kind != rv32i::RV32IMemoryAccessKind::Write || memory.fault) {
        return writes;
    }

    const size_t byte_count = memorySizeBytes(memory.size);
    for (size_t byte = 0; byte < byte_count; ++byte) {
        writes[static_cast<uint32_t>(memory.address + byte)] =
            static_cast<uint8_t>((memory.write_data >> (byte * 8)) & 0xFFU);
    }
    return writes;
}
}

BehavioralRV32ICore::BehavioralRV32ICore(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
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
          self->addPin("HALTED", PinType::OUTPUT);
          self->addPin("TRAPPED", PinType::OUTPUT);

          self->addPin<32>("IMEM_ADDR", PinType::OUTPUT);
          self->addPin<32>("IMEM_WRITE_DATA", PinType::OUTPUT);
          self->addPin("IMEM_READ_EN", PinType::OUTPUT);
          self->addPin("IMEM_WRITE_EN", PinType::OUTPUT);
          self->addPin<2>("IMEM_SIZE", PinType::OUTPUT);
          self->addPin("IMEM_SIGN_EXTEND", PinType::OUTPUT);
          self->addPin("IMEM_CLK", PinType::OUTPUT);
          self->addPin("IMEM_RST", PinType::OUTPUT);

          self->addPin<32>("DMEM_ADDR", PinType::OUTPUT);
          self->addPin<32>("DMEM_WRITE_DATA", PinType::OUTPUT);
          self->addPin("DMEM_READ_EN", PinType::OUTPUT);
          self->addPin("DMEM_WRITE_EN", PinType::OUTPUT);
          self->addPin<2>("DMEM_SIZE", PinType::OUTPUT);
          self->addPin("DMEM_SIGN_EXTEND", PinType::OUTPUT);
          self->addPin("DMEM_CLK", PinType::OUTPUT);
          self->addPin("DMEM_RST", PinType::OUTPUT);
      }) {
    resetCore();
}

void BehavioralRV32ICore::attachMemories(std::shared_ptr<BehavioralMemory64Kx32> instruction_memory,
                                         std::shared_ptr<BehavioralMemory64Kx32> data_memory) {
    instruction_memory_ = std::move(instruction_memory);
    data_memory_ = std::move(data_memory);
}

void BehavioralRV32ICore::setInitialPC(uint32_t pc) {
    reset_pc_ = pc;
    state_.pc = pc;
    last_instruction_address_ = pc;
}

void BehavioralRV32ICore::setRegister(uint8_t index, uint32_t value) {
    state_.writeRegister(index, value);
}

void BehavioralRV32ICore::resetCore() {
    state_ = rv32i::RV32IState{};
    state_.pc = reset_pc_;
    state_.forceX0();
    last_data_memory_access_ = rv32i::RV32IMemoryTrace{};
    last_data_memory_writes_.clear();
    last_instruction_address_ = reset_pc_;
    last_instruction_read_ = false;
}

rv32i::RV32IState BehavioralRV32ICore::snapshotState() const {
    return state_;
}

rv32i::RV32IMemoryTrace BehavioralRV32ICore::lastDataMemoryAccess() const {
    return last_data_memory_access_;
}

std::map<uint32_t, uint8_t> BehavioralRV32ICore::lastDataMemoryWrites() const {
    return last_data_memory_writes_;
}

void BehavioralRV32ICore::evaluate(size_t current_time, Simulator& simulator) {
    const auto clk = sanitizeBit(getInputValue("CLK"));
    const auto rst = sanitizeBit(getInputValue("RST"));
    const auto enable = sanitizeBit(getInputValue("ENABLE"));

    if (rst == LogicValue::HIGH) {
        resetCore();
    } else if (previous_clk_ == LogicValue::LOW && clk == LogicValue::HIGH && enable == LogicValue::HIGH) {
        auto instruction_memory = instruction_memory_.lock();
        auto data_memory = data_memory_.lock();
        if (instruction_memory && data_memory) {
            const bool will_fetch = !state_.halted && !state_.trapped;
            last_instruction_address_ = state_.pc;
            last_instruction_read_ = will_fetch;

            const auto trace = rv32i::RV32IInstructionOracle::step(state_, *instruction_memory, *data_memory);
            last_data_memory_access_ = trace.memory;
            last_data_memory_writes_ = expandDataMemoryWrites(trace.memory);
        } else {
            state_.trapped = true;
            state_.trap_cause = rv32i::RV32IExecutionTrapCause::InstructionAccessFault;
        }
    }

    previous_clk_ = clk;
    publishOutputs(simulator, current_time);
}

void BehavioralRV32ICore::publishOutputs(Simulator& simulator, size_t current_time) {
    _updateOutputWire<32>(simulator, "PC", state_.pc, current_time);
    _updateOutputWire(simulator, "HALTED", state_.halted ? LogicValue::HIGH : LogicValue::LOW, current_time);
    _updateOutputWire(simulator, "TRAPPED", state_.trapped ? LogicValue::HIGH : LogicValue::LOW, current_time);

    _updateOutputWire<32>(simulator, "IMEM_ADDR", last_instruction_address_, current_time);
    _updateOutputWire<32>(simulator, "IMEM_WRITE_DATA", 0, current_time);
    _updateOutputWire(simulator, "IMEM_READ_EN", last_instruction_read_ ? LogicValue::HIGH : LogicValue::LOW, current_time);
    _updateOutputWire(simulator, "IMEM_WRITE_EN", LogicValue::LOW, current_time);
    _updateOutputWire<2>(simulator, "IMEM_SIZE", encodeMemorySize(rv32i::RV32IMemorySize::Word), current_time);
    _updateOutputWire(simulator, "IMEM_SIGN_EXTEND", LogicValue::LOW, current_time);
    _updateOutputWire(simulator, "IMEM_CLK", LogicValue::LOW, current_time);
    _updateOutputWire(simulator, "IMEM_RST", LogicValue::LOW, current_time);

    const auto& memory = last_data_memory_access_;
    const bool data_read = memory.kind == rv32i::RV32IMemoryAccessKind::Read;
    const bool data_write = memory.kind == rv32i::RV32IMemoryAccessKind::Write;
    _updateOutputWire<32>(simulator, "DMEM_ADDR", data_read || data_write ? memory.address : 0, current_time);
    _updateOutputWire<32>(simulator, "DMEM_WRITE_DATA", data_write ? memory.write_data : 0, current_time);
    _updateOutputWire(simulator, "DMEM_READ_EN", data_read ? LogicValue::HIGH : LogicValue::LOW, current_time);
    _updateOutputWire(simulator, "DMEM_WRITE_EN", data_write ? LogicValue::HIGH : LogicValue::LOW, current_time);
    _updateOutputWire<2>(simulator, "DMEM_SIZE", encodeMemorySize(memory.size), current_time);
    _updateOutputWire(simulator, "DMEM_SIGN_EXTEND", memory.sign_extend ? LogicValue::HIGH : LogicValue::LOW, current_time);
    _updateOutputWire(simulator, "DMEM_CLK", LogicValue::LOW, current_time);
    _updateOutputWire(simulator, "DMEM_RST", LogicValue::LOW, current_time);
}
