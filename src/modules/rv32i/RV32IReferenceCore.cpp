#include "modules/rv32i/RV32IReferenceCore.hpp"

#include "basic/Pin.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/rv32i/RV32IComponentEncoding.hpp"
#include "modules/rv32i/RV32ISingleCycleCore.hpp"
#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IFunctionalMemory.hpp"
#include "rv32i/RV32IInstructionOracle.hpp"
#include "simulator/Simulator.hpp"
#include <optional>
#include <stdexcept>
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

std::optional<uint32_t> knownUIntIfPossible(
    const std::vector<LogicValue>& values) {
    uint32_t result = 0;
    for (size_t bit = 0; bit < values.size(); ++bit) {
        if (values[bit] != LogicValue::LOW
            && values[bit] != LogicValue::HIGH) {
            return std::nullopt;
        }
        if (values[bit] == LogicValue::HIGH) {
            result |= uint32_t{1} << bit;
        }
    }
    return result;
}

uint32_t requireKnownUInt(
    const std::vector<LogicValue>& values,
    const char* name) {
    const auto result = knownUIntIfPossible(values);
    if (!result) {
        throw std::logic_error(
            std::string(name)
            + " contains an unknown or high-Z bit");
    }
    return *result;
}

uint32_t evaluateAlu(
    uint8_t op,
    uint32_t a,
    uint32_t b) {
    const auto shamt = b & 0x1fU;
    switch (op) {
        case ALU32Op::ADD: return a + b;
        case ALU32Op::SUB: return a - b;
        case ALU32Op::AND: return a & b;
        case ALU32Op::OR: return a | b;
        case ALU32Op::XOR: return a ^ b;
        case ALU32Op::SLL: return a << shamt;
        case ALU32Op::SRL: return a >> shamt;
        case ALU32Op::SRA:
            return static_cast<uint32_t>(
                static_cast<int32_t>(a) >> shamt);
        case ALU32Op::SLT:
            return static_cast<int32_t>(a)
                    < static_cast<int32_t>(b)
                ? 1U : 0U;
        case ALU32Op::SLTU: return a < b ? 1U : 0U;
        case ALU32Op::PASS_A: return a;
        case ALU32Op::PASS_B: return b;
        case ALU32Op::ZERO: return 0;
        default: return 0;
    }
}

struct CorePreview {
    uint32_t raw = 0;
    rv32i::RV32IDecodedInstruction decoded{};
    rv32i::RV32IControlSignals control{};
    uint32_t rs1 = 0;
    uint32_t rs2 = 0;
    uint32_t alu_result = 0;
    bool memory_intent = false;
    bool active = false;
    bool attempt = false;
};

CorePreview previewCore(
    const rv32i::RV32IState& state,
    std::optional<uint32_t> raw,
    LogicValue rst,
    LogicValue enable,
    LogicValue imem_ready,
    LogicValue dmem_ready) {
    CorePreview result;
    result.active =
        rst == LogicValue::LOW
        && enable == LogicValue::HIGH
        && !state.halted
        && !state.trapped;
    if (!raw) {
        return result;
    }

    result.raw = *raw;
    result.decoded = rv32i::RV32IDecoder::decode(*raw);
    result.control = rv32i::RV32IControl::fromDecoded(
        result.decoded);
    result.rs1 = state.readRegister(result.decoded.rs1);
    result.rs2 = state.readRegister(result.decoded.rs2);
    const uint32_t alu_a =
        result.control.alu_a == rv32i::RV32IALUSourceA::RS1
        ? result.rs1
        : result.control.alu_a == rv32i::RV32IALUSourceA::PC
            ? state.pc
            : 0;
    const uint32_t alu_b =
        result.control.alu_b == rv32i::RV32IALUSourceB::RS2
        ? result.rs2
        : result.control.alu_b
                == rv32i::RV32IALUSourceB::Immediate
            ? static_cast<uint32_t>(result.decoded.immediate)
            : 0;
    result.alu_result =
        evaluateAlu(result.control.alu_op, alu_a, alu_b);
    result.memory_intent =
        result.control.mem_read || result.control.mem_write;
    const bool memory_response_ready =
        !result.memory_intent
        || dmem_ready == LogicValue::HIGH;
    result.attempt =
        result.active
        && imem_ready == LogicValue::HIGH
        && memory_response_ready;
    return result;
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

RV32IReferenceCore::RV32IReferenceCore(std::string name)
    : BasicComponent(
          std::move(name),
          1,
          circuit::families::RV32ISingleCycleCore.pinInitializer()) {
    resetCore();
}

void RV32IReferenceCore::setInitialPC(uint32_t pc) {
    reset_pc_ = pc;
    state_.pc = pc;
}

void RV32IReferenceCore::setRegister(uint8_t index, uint32_t value) {
    state_.writeRegister(index, value);
}

void RV32IReferenceCore::resetCore() {
    state_ = rv32i::RV32IState{};
    state_.pc = reset_pc_;
    state_.forceX0();
    last_data_memory_access_ = rv32i::RV32IMemoryTrace{};
    last_data_memory_writes_.clear();
}

rv32i::RV32IArchitecturalState
RV32IReferenceCore::snapshotArchitecturalState() const {
    return rv32i::RV32IArchitecturalState::fromKnown(state_);
}

rv32i::RV32IMemoryTrace RV32IReferenceCore::lastDataMemoryAccess() const {
    return last_data_memory_access_;
}

std::map<uint32_t, uint8_t> RV32IReferenceCore::lastDataMemoryWrites() const {
    return last_data_memory_writes_;
}

void RV32IReferenceCore::evaluate(size_t current_time, Simulator& simulator) {
    const auto clk = sanitizeBit(getInputValue("CLK"));
    const auto rst = sanitizeBit(getInputValue("RST"));
    const auto enable = sanitizeBit(getInputValue("ENABLE"));
    const auto imem_ready =
        sanitizeBit(getInputValue("IMEM_READY"));
    const auto dmem_ready =
        sanitizeBit(getInputValue("DMEM_READY"));
    const auto raw = knownUIntIfPossible(
        getInputPin<32>("IMEM_READ_DATA")
            ->getValueAsVector());
    const auto preview = previewCore(
        state_, raw, rst, enable, imem_ready, dmem_ready);
    const bool rising_edge =
        previous_clk_ == LogicValue::LOW
        && clk == LogicValue::HIGH;

    if (rst == LogicValue::HIGH) {
        resetCore();
    } else if (rising_edge && preview.attempt) {
        const bool imem_fault =
            getInputValue("IMEM_FAULT") == LogicValue::HIGH;
        const bool dmem_fault =
            getInputValue("DMEM_FAULT") == LogicValue::HIGH;

        rv32i::RV32IFunctionalMemory instruction_memory(
            imem_fault ? 0
                       : rv32i::RV32IFunctionalMemory::
                             DefaultCapacityBytes);
        if (!imem_fault) {
            instruction_memory.loadWords(
                state_.pc, {*raw});
        }
        rv32i::RV32IFunctionalMemory data_memory(
            dmem_fault && preview.memory_intent
                ? 0
                : rv32i::RV32IFunctionalMemory::
                      DefaultCapacityBytes);
        if (!dmem_fault && preview.control.mem_read) {
            const auto read_data = requireKnownUInt(
                getInputPin<32>("DMEM_READ_DATA")
                    ->getValueAsVector(),
                "DMEM_READ_DATA");
            switch (preview.control.mem_size) {
                case rv32i::RV32IMemorySize::Byte:
                    data_memory.writeU8(
                        preview.alu_result,
                        static_cast<uint8_t>(read_data));
                    break;
                case rv32i::RV32IMemorySize::Halfword:
                    data_memory.writeU16(
                        preview.alu_result,
                        static_cast<uint16_t>(read_data));
                    break;
                case rv32i::RV32IMemorySize::Word:
                    data_memory.writeU32(
                        preview.alu_result, read_data);
                    break;
                case rv32i::RV32IMemorySize::None:
                    break;
            }
        }
        const auto trace =
            rv32i::RV32IInstructionOracle::step(
                state_, instruction_memory, data_memory);
        last_data_memory_access_ = trace.memory;
        last_data_memory_writes_ =
            expandDataMemoryWrites(trace.memory);
    }

    previous_clk_ = clk;
    publishOutputs(simulator, current_time);
}

void RV32IReferenceCore::publishOutputs(
    Simulator& simulator,
    size_t current_time) {
    const auto rst = sanitizeBit(getInputValue("RST"));
    const auto enable = sanitizeBit(getInputValue("ENABLE"));
    const auto imem_ready =
        sanitizeBit(getInputValue("IMEM_READY"));
    const auto dmem_ready =
        sanitizeBit(getInputValue("DMEM_READY"));
    const auto raw = knownUIntIfPossible(
        getInputPin<32>("IMEM_READ_DATA")
            ->getValueAsVector());
    const auto preview = previewCore(
        state_, raw, rst, enable, imem_ready, dmem_ready);

    _updateOutputWire<32>(simulator, "PC", state_.pc, current_time);
    _updateOutputWire(simulator, "HALTED", state_.halted ? LogicValue::HIGH : LogicValue::LOW, current_time);
    _updateOutputWire(simulator, "TRAPPED", state_.trapped ? LogicValue::HIGH : LogicValue::LOW, current_time);
    _updateOutputWire<4>(
        simulator,
        "TRAP_CAUSE",
        rv32i::component_encoding::executionTrapCause(
            state_.trap_cause),
        current_time);
    _updateOutputWire(
        simulator,
        "INSTRUCTION_ATTEMPT",
        preview.attempt
            ? LogicValue::HIGH
            : LogicValue::LOW,
        current_time);

    _updateOutputWire<32>(
        simulator, "IMEM_ADDR", state_.pc, current_time);
    _updateOutputWire(
        simulator, "IMEM_READ_EN",
        LogicValue::HIGH, current_time);

    const bool data_request =
        preview.active
        && imem_ready == LogicValue::HIGH
        && preview.control.legal
        && preview.memory_intent;
    if (raw) {
        _updateOutputWire<32>(
            simulator, "DMEM_ADDR",
            preview.alu_result, current_time);
    } else {
        _updateOutputWire<32>(
            simulator,
            "DMEM_ADDR",
            std::vector<LogicValue>(
                32, LogicValue::UNKNOWN),
            current_time);
    }
    _updateOutputWire<32>(
        simulator, "DMEM_WRITE_DATA",
        preview.rs2, current_time);
    _updateOutputWire(
        simulator, "DMEM_READ_EN",
        data_request && preview.control.mem_read
            ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    _updateOutputWire(
        simulator, "DMEM_WRITE_EN",
        data_request && preview.control.mem_write
            ? LogicValue::HIGH : LogicValue::LOW,
        current_time);
    if (raw) {
        _updateOutputWire<2>(
            simulator, "DMEM_SIZE",
            encodeMemorySize(preview.control.mem_size),
            current_time);
    } else {
        _updateOutputWire<2>(
            simulator,
            "DMEM_SIZE",
            std::vector<LogicValue>(
                2, LogicValue::UNKNOWN),
            current_time);
    }
    _updateOutputWire(
        simulator, "DMEM_SIGN_EXTEND",
        raw
            ? (preview.control.load_sign_extend
                ? LogicValue::HIGH
                : LogicValue::LOW)
            : LogicValue::UNKNOWN,
        current_time);
}
