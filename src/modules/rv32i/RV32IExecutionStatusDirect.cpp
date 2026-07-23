#include "modules/rv32i/RV32IExecutionStatusDirect.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"

#include "modules/rv32i/RV32IComponentEncoding.hpp"
#include "rv32i/RV32IState.hpp"
#include "simulator/Simulator.hpp"
#include <cstdint>
#include <utility>

namespace {
bool isKnown(LogicValue value) {
    return value == LogicValue::LOW || value == LogicValue::HIGH;
}

bool allKnown(const std::vector<LogicValue>& values) {
    for (const auto value : values) {
        if (!isKnown(value)) {
            return false;
        }
    }
    return true;
}

bool high(LogicValue value) {
    return value == LogicValue::HIGH;
}

LogicValue bit(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}

uint32_t toUInt32(const std::vector<LogicValue>& values) {
    uint32_t result = 0;
    for (size_t index = 0; index < values.size() && index < 32; ++index) {
        if (values[index] == LogicValue::HIGH) {
            result |= uint32_t{1} << index;
        }
    }
    return result;
}

std::vector<LogicValue> bits4(uint8_t value) {
    std::vector<LogicValue> result(4, LogicValue::LOW);
    for (size_t bit_index = 0; bit_index < result.size(); ++bit_index) {
        result[bit_index] = bit(((value >> bit_index) & 1U) != 0);
    }
    return result;
}

uint8_t cause(rv32i::RV32IExecutionTrapCause value) {
    return rv32i::component_encoding::executionTrapCause(value);
}

struct Inputs {
    LogicValue rst;
    LogicValue enable;
    LogicValue legal;
    LogicValue reg_write;
    LogicValue mem_read;
    LogicValue mem_write;
    LogicValue halt_request;
    LogicValue trap_request;
    std::vector<LogicValue> decode_trap_cause;
    std::vector<LogicValue> alu_address;
    std::vector<LogicValue> mem_size;
    LogicValue pc_misaligned;
    LogicValue target_misaligned;
    LogicValue imem_ready;
    LogicValue imem_fault;
    LogicValue dmem_ready;
    LogicValue dmem_fault;

    bool known() const {
        return isKnown(rst) && isKnown(enable) && isKnown(legal)
            && isKnown(reg_write) && isKnown(mem_read) && isKnown(mem_write)
            && isKnown(halt_request) && isKnown(trap_request)
            && allKnown(decode_trap_cause) && allKnown(alu_address)
            && allKnown(mem_size) && isKnown(pc_misaligned)
            && isKnown(target_misaligned) && isKnown(imem_ready)
            && isKnown(imem_fault) && isKnown(dmem_ready) && isKnown(dmem_fault);
    }
};

struct CombinationalResult {
    LogicValue pc_write = LogicValue::UNKNOWN;
    LogicValue register_write = LogicValue::UNKNOWN;
    LogicValue memory_request = LogicValue::UNKNOWN;
    LogicValue data_address_misaligned = LogicValue::UNKNOWN;
    LogicValue instruction_attempt = LogicValue::UNKNOWN;
    bool halt_event = false;
    bool trap_event = false;
    uint8_t event_cause = 0;
};

CombinationalResult compute(const Inputs& input,
                            LogicValue halted,
                            LogicValue trapped) {
    CombinationalResult result;
    if (!input.known() || !isKnown(halted) || !isKnown(trapped)) {
        return result;
    }

    const uint32_t address = toUInt32(input.alu_address);
    const uint8_t size = static_cast<uint8_t>(toUInt32(input.mem_size));
    const bool data_misaligned = size == 3
        || (size == 1 && (address & 0x1U) != 0)
        || (size == 2 && (address & 0x3U) != 0);
    result.data_address_misaligned = bit(data_misaligned);

    const bool active = !high(input.rst) && high(input.enable)
        && !high(halted) && !high(trapped);
    const bool memory_intent = high(input.mem_read) || high(input.mem_write);
    const bool memory_request = active && high(input.imem_ready)
        && high(input.legal) && !high(input.pc_misaligned)
        && !high(input.imem_fault) && !high(input.trap_request)
        && !high(input.halt_request) && memory_intent;
    const bool attempt = active && high(input.imem_ready)
        && (!memory_request || high(input.dmem_ready));

    result.memory_request = bit(memory_request);
    result.instruction_attempt = bit(attempt);

    bool normal = attempt;
    auto trapIf = [&](bool condition, uint8_t event_cause) {
        if (normal && condition) {
            normal = false;
            result.trap_event = true;
            result.event_cause = event_cause;
        }
    };

    trapIf(high(input.pc_misaligned),
           cause(rv32i::RV32IExecutionTrapCause::InstructionAddressMisaligned));
    trapIf(high(input.imem_fault),
           cause(rv32i::RV32IExecutionTrapCause::InstructionAccessFault));
    if (normal && (!high(input.legal) || high(input.trap_request))) {
        normal = false;
        result.trap_event = true;
        result.event_cause = high(input.legal)
            ? static_cast<uint8_t>(toUInt32(input.decode_trap_cause))
            : cause(rv32i::RV32IExecutionTrapCause::IllegalInstruction);
    }
    if (normal && high(input.halt_request)) {
        normal = false;
        result.halt_event = true;
    }
    trapIf(memory_request && data_misaligned,
           high(input.mem_write)
               ? cause(rv32i::RV32IExecutionTrapCause::StoreAddressMisaligned)
               : cause(rv32i::RV32IExecutionTrapCause::LoadAddressMisaligned));
    trapIf(memory_request && high(input.dmem_fault),
           high(input.mem_write)
               ? cause(rv32i::RV32IExecutionTrapCause::StoreAccessFault)
               : cause(rv32i::RV32IExecutionTrapCause::LoadAccessFault));
    trapIf(high(input.target_misaligned),
           cause(rv32i::RV32IExecutionTrapCause::InstructionAddressMisaligned));

    result.pc_write = bit(normal);
    result.register_write = bit(normal && high(input.reg_write));
    return result;
}
}

RV32IExecutionStatusDirect::RV32IExecutionStatusDirect(
    std::string name)
    : BasicComponent(
          std::move(name), 1,
          circuit::families::RV32IExecutionStatus.pinInitializer()) {}

void RV32IExecutionStatusDirect::evaluate(size_t current_time,
                                                          Simulator& simulator) {
    const Inputs input{
        getInputValue("RST"), getInputValue("ENABLE"), getInputValue("LEGAL"),
        getInputValue("REG_WRITE"), getInputValue("MEM_READ"),
        getInputValue("MEM_WRITE"), getInputValue("HALT_REQUEST"),
        getInputValue("TRAP_REQUEST"),
        getInputPin<4>("DECODE_TRAP_CAUSE")->getValueAsVector(),
        getInputPin<32>("ALU_ADDRESS")->getValueAsVector(),
        getInputPin<2>("MEM_SIZE")->getValueAsVector(),
        getInputValue("PC_MISALIGNED"), getInputValue("TARGET_MISALIGNED"),
        getInputValue("IMEM_READY"), getInputValue("IMEM_FAULT"),
        getInputValue("DMEM_READY"), getInputValue("DMEM_FAULT")};
    const auto clk = getInputValue("CLK");

    if (input.rst == LogicValue::HIGH) {
        halted_ = LogicValue::LOW;
        trapped_ = LogicValue::LOW;
        trap_cause_ = bits4(cause(rv32i::RV32IExecutionTrapCause::None));
    } else if (input.rst != LogicValue::LOW) {
        halted_ = LogicValue::UNKNOWN;
        trapped_ = LogicValue::UNKNOWN;
        trap_cause_.assign(4, LogicValue::UNKNOWN);
    } else if (previous_clk_ == LogicValue::LOW && clk == LogicValue::HIGH) {
        const auto event = compute(input, halted_, trapped_);
        if (event.trap_event) {
            trapped_ = LogicValue::HIGH;
            trap_cause_ = bits4(event.event_cause);
        } else if (event.halt_event) {
            halted_ = LogicValue::HIGH;
        }
    }

    previous_clk_ = clk;
    const auto result = compute(input, halted_, trapped_);
    _updateOutputWire(simulator, "PC_WRITE", result.pc_write, current_time);
    _updateOutputWire(simulator, "REGISTER_WRITE", result.register_write, current_time);
    _updateOutputWire(simulator, "MEMORY_REQUEST_ACTIVE", result.memory_request, current_time);
    _updateOutputWire(simulator, "HALTED", halted_, current_time);
    _updateOutputWire(simulator, "TRAPPED", trapped_, current_time);
    _updateOutputWire<4>(simulator, "TRAP_CAUSE", trap_cause_, current_time);
    _updateOutputWire(simulator, "DATA_ADDRESS_MISALIGNED",
                      result.data_address_misaligned, current_time);
    _updateOutputWire(simulator, "INSTRUCTION_ATTEMPT",
                      result.instruction_attempt, current_time);
}
