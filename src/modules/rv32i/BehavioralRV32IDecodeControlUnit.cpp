#include "modules/rv32i/BehavioralRV32IDecodeControlUnit.hpp"

#include "modules/rv32i/RV32IComponentEncoding.hpp"
#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "simulator/Simulator.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace {
bool allKnown(const std::vector<LogicValue>& values) {
    for (const auto value : values) {
        if (value != LogicValue::LOW && value != LogicValue::HIGH) {
            return false;
        }
    }
    return true;
}

LogicValue logic(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
}
}

BehavioralRV32IDecodeControlUnit::BehavioralRV32IDecodeControlUnit(std::string name)
    : BasicComponent(std::move(name), 1, [](IOComponent* self) {
          self->addPin<32>("INSTRUCTION", PinType::INPUT);
          self->addPin<5>("RS1_ADDR", PinType::OUTPUT);
          self->addPin<5>("RS2_ADDR", PinType::OUTPUT);
          self->addPin<5>("RD_ADDR", PinType::OUTPUT);
          self->addPin<32>("IMM", PinType::OUTPUT);
          self->addPin<5>("ALU_OP", PinType::OUTPUT);
          self->addPin<2>("ALU_A_SEL", PinType::OUTPUT);
          self->addPin<2>("ALU_B_SEL", PinType::OUTPUT);
          self->addPin("LEGAL", PinType::OUTPUT);
          self->addPin("REG_WRITE", PinType::OUTPUT);
          self->addPin("MEM_READ", PinType::OUTPUT);
          self->addPin("MEM_WRITE", PinType::OUTPUT);
          self->addPin<2>("WRITEBACK_SEL", PinType::OUTPUT);
          self->addPin<2>("MEM_SIZE", PinType::OUTPUT);
          self->addPin("LOAD_SIGN_EXTEND", PinType::OUTPUT);
          self->addPin<3>("BRANCH_TYPE", PinType::OUTPUT);
          self->addPin<2>("JUMP_TYPE", PinType::OUTPUT);
          self->addPin("HALT_REQUEST", PinType::OUTPUT);
          self->addPin("TRAP_REQUEST", PinType::OUTPUT);
          self->addPin<4>("DECODE_TRAP_CAUSE", PinType::OUTPUT);
      }) {}

void BehavioralRV32IDecodeControlUnit::evaluate(size_t current_time, Simulator& simulator) {
    const auto input = getInputPin<32>("INSTRUCTION")->getValueAsVector();
    if (!allKnown(input)) {
        const std::vector<LogicValue> unknown2(2, LogicValue::UNKNOWN);
        const std::vector<LogicValue> unknown3(3, LogicValue::UNKNOWN);
        const std::vector<LogicValue> unknown4(4, LogicValue::UNKNOWN);
        const std::vector<LogicValue> unknown5(5, LogicValue::UNKNOWN);
        const std::vector<LogicValue> unknown32(32, LogicValue::UNKNOWN);
        _updateOutputWire<5>(simulator, "RS1_ADDR", unknown5, current_time);
        _updateOutputWire<5>(simulator, "RS2_ADDR", unknown5, current_time);
        _updateOutputWire<5>(simulator, "RD_ADDR", unknown5, current_time);
        _updateOutputWire<32>(simulator, "IMM", unknown32, current_time);
        _updateOutputWire<5>(simulator, "ALU_OP", unknown5, current_time);
        _updateOutputWire<2>(simulator, "ALU_A_SEL", unknown2, current_time);
        _updateOutputWire<2>(simulator, "ALU_B_SEL", unknown2, current_time);
        _updateOutputWire(simulator, "LEGAL", LogicValue::UNKNOWN, current_time);
        _updateOutputWire(simulator, "REG_WRITE", LogicValue::UNKNOWN, current_time);
        _updateOutputWire(simulator, "MEM_READ", LogicValue::UNKNOWN, current_time);
        _updateOutputWire(simulator, "MEM_WRITE", LogicValue::UNKNOWN, current_time);
        _updateOutputWire<2>(simulator, "WRITEBACK_SEL", unknown2, current_time);
        _updateOutputWire<2>(simulator, "MEM_SIZE", unknown2, current_time);
        _updateOutputWire(simulator, "LOAD_SIGN_EXTEND", LogicValue::UNKNOWN, current_time);
        _updateOutputWire<3>(simulator, "BRANCH_TYPE", unknown3, current_time);
        _updateOutputWire<2>(simulator, "JUMP_TYPE", unknown2, current_time);
        _updateOutputWire(simulator, "HALT_REQUEST", LogicValue::UNKNOWN, current_time);
        _updateOutputWire(simulator, "TRAP_REQUEST", LogicValue::UNKNOWN, current_time);
        _updateOutputWire<4>(simulator, "DECODE_TRAP_CAUSE", unknown4, current_time);
        return;
    }

    const uint32_t raw = static_cast<uint32_t>(getInputPin<32>("INSTRUCTION")->getValueAsUInt64());
    const auto decoded = rv32i::RV32IDecoder::decode(raw);
    const auto control = rv32i::RV32IControl::fromDecoded(decoded);

    _updateOutputWire<5>(simulator, "RS1_ADDR", decoded.rs1, current_time);
    _updateOutputWire<5>(simulator, "RS2_ADDR", decoded.rs2, current_time);
    _updateOutputWire<5>(simulator, "RD_ADDR", decoded.rd, current_time);
    _updateOutputWire<32>(simulator, "IMM", static_cast<uint32_t>(decoded.immediate), current_time);
    _updateOutputWire<5>(simulator, "ALU_OP", control.alu_op, current_time);
    _updateOutputWire<2>(simulator, "ALU_A_SEL", rv32i::component_encoding::aluSourceA(control.alu_a), current_time);
    _updateOutputWire<2>(simulator, "ALU_B_SEL", rv32i::component_encoding::aluSourceB(control.alu_b), current_time);
    _updateOutputWire(simulator, "LEGAL", logic(control.legal), current_time);
    _updateOutputWire(simulator, "REG_WRITE", logic(control.reg_write), current_time);
    _updateOutputWire(simulator, "MEM_READ", logic(control.mem_read), current_time);
    _updateOutputWire(simulator, "MEM_WRITE", logic(control.mem_write), current_time);
    _updateOutputWire<2>(simulator, "WRITEBACK_SEL", rv32i::component_encoding::writeback(control.writeback), current_time);
    _updateOutputWire<2>(simulator, "MEM_SIZE", rv32i::component_encoding::memorySize(control.mem_size), current_time);
    _updateOutputWire(simulator, "LOAD_SIGN_EXTEND", logic(control.load_sign_extend), current_time);
    _updateOutputWire<3>(simulator, "BRANCH_TYPE", rv32i::component_encoding::branch(control.branch), current_time);
    _updateOutputWire<2>(simulator, "JUMP_TYPE", rv32i::component_encoding::jump(control.jump), current_time);
    _updateOutputWire(simulator, "HALT_REQUEST", logic(control.halt), current_time);
    _updateOutputWire(simulator, "TRAP_REQUEST", logic(control.trap), current_time);
    _updateOutputWire<4>(simulator, "DECODE_TRAP_CAUSE", rv32i::component_encoding::decodeTrapCause(control.trap_cause), current_time);
}
