#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "modules/rv32i/RV32IExecutionStatusDirect.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/memory/MemoryBit.hpp"
#include "modules/rv32i/RV32IComponentEncoding.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "rv32i/RV32IState.hpp"
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
using NetId = size_t;

struct LogicNet {
    std::string name;
    std::shared_ptr<Pin<>> source;
    std::vector<std::shared_ptr<Pin<>>> sinks;
};

class LogicNetlist {
public:
    explicit LogicNetlist(ComponentBuilder& builder) : builder_(builder) {}

    NetId source(std::string name, std::shared_ptr<Pin<>> pin) {
        nets_.push_back({std::move(name), std::move(pin), {}});
        return nets_.size() - 1;
    }

    void sink(NetId net, std::shared_ptr<Pin<>> pin) {
        nets_.at(net).sinks.push_back(std::move(pin));
    }

    NetId logicalNot(const std::string& name, NetId input) {
        builder_.addNewComponent<NOTGate>(name);
        sink(input, builder_.getInputPin<NOTGate>(name, "IN"));
        return source(name + "_OUT", builder_.getOutputPin<NOTGate>(name, "OUT"));
    }

    NetId logicalAnd(const std::string& name, NetId left, NetId right) {
        builder_.addNewComponent<ANDGate>(name);
        sink(left, builder_.getInputPin<ANDGate>(name, "A"));
        sink(right, builder_.getInputPin<ANDGate>(name, "B"));
        return source(name + "_OUT", builder_.getOutputPin<ANDGate>(name, "OUT"));
    }

    NetId logicalOr(const std::string& name, NetId left, NetId right) {
        builder_.addNewComponent<ORGate>(name);
        sink(left, builder_.getInputPin<ORGate>(name, "A"));
        sink(right, builder_.getInputPin<ORGate>(name, "B"));
        return source(name + "_OUT", builder_.getOutputPin<ORGate>(name, "OUT"));
    }

    NetId andAll(const std::string& prefix, const std::vector<NetId>& inputs) {
        NetId current = inputs.front();
        for (size_t index = 1; index < inputs.size(); ++index) {
            current = logicalAnd(prefix + "_" + std::to_string(index), current, inputs[index]);
        }
        return current;
    }

    NetId orAll(const std::string& prefix, const std::vector<NetId>& inputs) {
        NetId current = inputs.front();
        for (size_t index = 1; index < inputs.size(); ++index) {
            current = logicalOr(prefix + "_" + std::to_string(index), current, inputs[index]);
        }
        return current;
    }

    std::pair<NetId, NetId> priorityStage(const std::string& name,
                                          NetId permission,
                                          NetId condition) {
        const auto event = logicalAnd(name + "_EVENT", permission, condition);
        const auto not_condition = logicalNot(name + "_NOT_CONDITION", condition);
        const auto next_permission = logicalAnd(name + "_ALLOW_NEXT", permission, not_condition);
        return {event, next_permission};
    }

    void materialize() {
        for (auto& net : nets_) {
            builder_.addNewWire(net.name, net.source, net.sinks);
        }
    }

private:
    ComponentBuilder& builder_;
    std::vector<LogicNet> nets_;
};

uint8_t cause(rv32i::RV32IExecutionTrapCause value) {
    return rv32i::component_encoding::executionTrapCause(value);
}
}
namespace {
void defineExecutionStatusPins(IOComponent* self) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("ENABLE", PinType::INPUT);
    self->addPin("LEGAL", PinType::INPUT);
    self->addPin("REG_WRITE", PinType::INPUT);
    self->addPin("MEM_READ", PinType::INPUT);
    self->addPin("MEM_WRITE", PinType::INPUT);
    self->addPin("HALT_REQUEST", PinType::INPUT);
    self->addPin("TRAP_REQUEST", PinType::INPUT);
    self->addPin<4>("DECODE_TRAP_CAUSE", PinType::INPUT);
    self->addPin<32>("ALU_ADDRESS", PinType::INPUT);
    self->addPin<2>("MEM_SIZE", PinType::INPUT);
    self->addPin("PC_MISALIGNED", PinType::INPUT);
    self->addPin("TARGET_MISALIGNED", PinType::INPUT);
    self->addPin("IMEM_READY", PinType::INPUT);
    self->addPin("IMEM_FAULT", PinType::INPUT);
    self->addPin("DMEM_READY", PinType::INPUT);
    self->addPin("DMEM_FAULT", PinType::INPUT);
    self->addPin("PC_WRITE", PinType::OUTPUT);
    self->addPin("REGISTER_WRITE", PinType::OUTPUT);
    self->addPin("MEMORY_REQUEST_ACTIVE", PinType::OUTPUT);
    self->addPin("HALTED", PinType::OUTPUT);
    self->addPin("TRAPPED", PinType::OUTPUT);
    self->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
    self->addPin("DATA_ADDRESS_MISALIGNED", PinType::OUTPUT);
    self->addPin("INSTRUCTION_ATTEMPT", PinType::OUTPUT);
}
}

namespace circuit::families {
const ComponentFamily RV32IExecutionStatus{
    "rv32i.execution-status",
    "RV32IExecutionControlStatusUnit",
    defineExecutionStatusPins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IExecutionControlStatusUnit>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IExecutionStatusDirect>(context, name);
    }};
}

RV32IExecutionControlStatusUnit::RV32IExecutionControlStatusUnit(std::string name)
    : IOComponent(std::move(name),
                  circuit::families::RV32IExecutionStatus.pinInitializer()) {}

void RV32IExecutionControlStatusUnit::buildInternals(ComponentBuilder& builder) {
    builder.add(circuit::families::MemoryBit, "HALTED_STATE");
    builder.add(circuit::families::MemoryBit, "TRAPPED_STATE");
    builder.addNewComponent<BitSplitter<32>>("ADDRESS_SPLIT");
    builder.addNewComponent<BitSplitter<2>>("SIZE_SPLIT");
    builder.addNewComponent<BitSplitter<4>>("CAUSE_D_SPLIT");
    builder.addNewComponent<BitJoiner<4>>("CAUSE_Q_JOIN");
    builder.addNewComponent<ConstantValue<1>>("CONST_HIGH", 1);

    for (size_t bit = 0; bit < 4; ++bit) {
        const auto name = "TRAP_CAUSE_BIT_" + std::to_string(bit);
        builder.add(circuit::families::MemoryBit, name);
    }

    builder.addNewComponent<ConstantValue<4>>("CAUSE_NONE", cause(rv32i::RV32IExecutionTrapCause::None));
    builder.addNewComponent<ConstantValue<4>>("CAUSE_ILLEGAL", cause(rv32i::RV32IExecutionTrapCause::IllegalInstruction));
    builder.addNewComponent<ConstantValue<4>>("CAUSE_INSTRUCTION_MISALIGNED", cause(rv32i::RV32IExecutionTrapCause::InstructionAddressMisaligned));
    builder.addNewComponent<ConstantValue<4>>("CAUSE_INSTRUCTION_ACCESS", cause(rv32i::RV32IExecutionTrapCause::InstructionAccessFault));
    builder.addNewComponent<ConstantValue<4>>("CAUSE_LOAD_MISALIGNED", cause(rv32i::RV32IExecutionTrapCause::LoadAddressMisaligned));
    builder.addNewComponent<ConstantValue<4>>("CAUSE_LOAD_ACCESS", cause(rv32i::RV32IExecutionTrapCause::LoadAccessFault));
    builder.addNewComponent<ConstantValue<4>>("CAUSE_STORE_MISALIGNED", cause(rv32i::RV32IExecutionTrapCause::StoreAddressMisaligned));
    builder.addNewComponent<ConstantValue<4>>("CAUSE_STORE_ACCESS", cause(rv32i::RV32IExecutionTrapCause::StoreAccessFault));

    builder.addNewComponent<Mux2to1_4bit>("DECODE_CAUSE_MUX");
    builder.addNewComponent<Mux2to1_4bit>("DATA_MISALIGNED_CAUSE_MUX");
    builder.addNewComponent<Mux2to1_4bit>("DATA_ACCESS_CAUSE_MUX");
    builder.addNewComponent<Mux2to1_4bit>("CAUSE_CONTROL_MUX");
    builder.addNewComponent<Mux2to1_4bit>("CAUSE_IMEM_MUX");
    builder.addNewComponent<Mux2to1_4bit>("CAUSE_DECODE_MUX");
    builder.addNewComponent<Mux2to1_4bit>("CAUSE_DATA_MISALIGNED_MUX");
    builder.addNewComponent<Mux2to1_4bit>("CAUSE_DATA_ACCESS_MUX");

    builder.addNewWire<32>(
        "ALU_ADDRESS_to_split",
        getInputPin<32>("ALU_ADDRESS"),
        {builder.getInputPin<BitSplitter<32>, 32>("ADDRESS_SPLIT", "IN")});
    builder.addNewWire<2>(
        "MEM_SIZE_to_split",
        getInputPin<2>("MEM_SIZE"),
        {builder.getInputPin<BitSplitter<2>, 2>("SIZE_SPLIT", "IN")});

    LogicNetlist logic(builder);
    const auto clk = logic.source("CLK_internal", getInputPin("CLK"));
    const auto rst = logic.source("RST_internal", getInputPin("RST"));
    const auto enable = logic.source("ENABLE_internal", getInputPin("ENABLE"));
    const auto legal = logic.source("LEGAL_internal", getInputPin("LEGAL"));
    const auto reg_write = logic.source("REG_WRITE_internal", getInputPin("REG_WRITE"));
    const auto mem_read = logic.source("MEM_READ_internal", getInputPin("MEM_READ"));
    const auto mem_write = logic.source("MEM_WRITE_internal", getInputPin("MEM_WRITE"));
    const auto halt_request = logic.source("HALT_REQUEST_internal", getInputPin("HALT_REQUEST"));
    const auto trap_request = logic.source("TRAP_REQUEST_internal", getInputPin("TRAP_REQUEST"));
    const auto pc_misaligned = logic.source("PC_MISALIGNED_internal", getInputPin("PC_MISALIGNED"));
    const auto target_misaligned = logic.source("TARGET_MISALIGNED_internal", getInputPin("TARGET_MISALIGNED"));
    const auto imem_ready = logic.source("IMEM_READY_internal", getInputPin("IMEM_READY"));
    const auto imem_fault = logic.source("IMEM_FAULT_internal", getInputPin("IMEM_FAULT"));
    const auto dmem_ready = logic.source("DMEM_READY_internal", getInputPin("DMEM_READY"));
    const auto dmem_fault = logic.source("DMEM_FAULT_internal", getInputPin("DMEM_FAULT"));
    const auto address_bit0 = logic.source(
        "ADDRESS_BIT_0", builder.getOutputPin<BitSplitter<32>>("ADDRESS_SPLIT", "OUT_0"));
    const auto address_bit1 = logic.source(
        "ADDRESS_BIT_1", builder.getOutputPin<BitSplitter<32>>("ADDRESS_SPLIT", "OUT_1"));
    const auto size_bit0 = logic.source(
        "SIZE_BIT_0", builder.getOutputPin<BitSplitter<2>>("SIZE_SPLIT", "OUT_0"));
    const auto size_bit1 = logic.source(
        "SIZE_BIT_1", builder.getOutputPin<BitSplitter<2>>("SIZE_SPLIT", "OUT_1"));
    const auto high = logic.source(
        "CONST_HIGH_fanout", builder.getOutputPin<ConstantValue<1>>("CONST_HIGH", "OUT"));
    const auto halted = logic.source(
        "HALTED_STATE_Q", builder.getOutputPin<IOComponent>("HALTED_STATE", "Q"));
    const auto trapped = logic.source(
        "TRAPPED_STATE_Q", builder.getOutputPin<IOComponent>("TRAPPED_STATE", "Q"));

    logic.sink(clk, builder.getInputPin<IOComponent>("HALTED_STATE", "CLK"));
    logic.sink(clk, builder.getInputPin<IOComponent>("TRAPPED_STATE", "CLK"));
    logic.sink(rst, builder.getInputPin<IOComponent>("HALTED_STATE", "RST"));
    logic.sink(rst, builder.getInputPin<IOComponent>("TRAPPED_STATE", "RST"));
    logic.sink(high, builder.getInputPin<IOComponent>("HALTED_STATE", "D"));
    logic.sink(high, builder.getInputPin<IOComponent>("TRAPPED_STATE", "D"));
    logic.sink(halted, getOutputPin("HALTED"));
    logic.sink(trapped, getOutputPin("TRAPPED"));

    for (size_t bit = 0; bit < 4; ++bit) {
        const auto cell = "TRAP_CAUSE_BIT_" + std::to_string(bit);
        logic.sink(clk, builder.getInputPin<IOComponent>(cell, "CLK"));
        logic.sink(rst, builder.getInputPin<IOComponent>(cell, "RST"));
        builder.addNewWire(
            "CAUSE_D_bit_" + std::to_string(bit),
            builder.getOutputPin<BitSplitter<4>>("CAUSE_D_SPLIT", "OUT_" + std::to_string(bit)),
            {builder.getInputPin<IOComponent>(cell, "D")});
        builder.addNewWire(
            "CAUSE_Q_bit_" + std::to_string(bit),
            builder.getOutputPin<IOComponent>(cell, "Q"),
            {builder.getInputPin<BitJoiner<4>>("CAUSE_Q_JOIN", "IN_" + std::to_string(bit))});
    }
    builder.addNewWire<4>(
        "CAUSE_Q_to_output",
        builder.getOutputPin<BitJoiner<4>, 4>("CAUSE_Q_JOIN", "OUT"),
        {getOutputPin<4>("TRAP_CAUSE")});

    const auto address_low = logic.logicalOr("ADDRESS_LOW_OR", address_bit0, address_bit1);
    const auto half_misaligned = logic.logicalAnd("HALF_MISALIGNED", size_bit0, address_bit0);
    const auto word_misaligned = logic.logicalAnd("WORD_MISALIGNED", size_bit1, address_low);
    const auto invalid_size = logic.logicalAnd("INVALID_SIZE", size_bit0, size_bit1);
    const auto any_size_misaligned = logic.logicalOr("SIZE_MISALIGNED_OR", half_misaligned, word_misaligned);
    const auto data_address_misaligned = logic.logicalOr(
        "DATA_ADDRESS_MISALIGNED_OR", any_size_misaligned, invalid_size);
    logic.sink(data_address_misaligned, getOutputPin("DATA_ADDRESS_MISALIGNED"));

    const auto not_halted = logic.logicalNot("NOT_HALTED", halted);
    const auto not_trapped = logic.logicalNot("NOT_TRAPPED", trapped);
    const auto not_reset = logic.logicalNot("NOT_RESET", rst);
    const auto active = logic.andAll("ACTIVE", {enable, not_halted, not_trapped, not_reset});
    const auto not_pc_misaligned = logic.logicalNot("NOT_PC_MISALIGNED", pc_misaligned);
    const auto not_imem_fault = logic.logicalNot("NOT_IMEM_FAULT", imem_fault);
    const auto not_trap_request = logic.logicalNot("NOT_TRAP_REQUEST", trap_request);
    const auto not_halt_request = logic.logicalNot("NOT_HALT_REQUEST", halt_request);
    const auto memory_intent = logic.logicalOr("MEMORY_INTENT", mem_read, mem_write);
    const auto memory_request = logic.andAll(
        "MEMORY_REQUEST",
        {active, imem_ready, legal, not_pc_misaligned, not_imem_fault,
         not_trap_request, not_halt_request, memory_intent});
    logic.sink(memory_request, getOutputPin("MEMORY_REQUEST_ACTIVE"));

    const auto not_memory_request = logic.logicalNot("NOT_MEMORY_REQUEST", memory_request);
    const auto data_response_ready = logic.logicalOr(
        "DATA_RESPONSE_READY", not_memory_request, dmem_ready);
    const auto fetch_ready = logic.logicalAnd("FETCH_READY", active, imem_ready);
    const auto instruction_attempt = logic.logicalAnd(
        "INSTRUCTION_ATTEMPT_GATE", fetch_ready, data_response_ready);
    logic.sink(instruction_attempt, getOutputPin("INSTRUCTION_ATTEMPT"));

    const auto not_legal = logic.logicalNot("NOT_LEGAL", legal);
    const auto decode_trap_condition = logic.logicalOr(
        "DECODE_TRAP_CONDITION", not_legal, trap_request);
    const auto data_misaligned_condition = logic.logicalAnd(
        "DATA_MISALIGNED_CONDITION", memory_request, data_address_misaligned);
    const auto data_fault_condition = logic.logicalAnd(
        "DATA_FAULT_CONDITION", memory_request, dmem_fault);

    auto permission = instruction_attempt;
    const auto [pc_event, after_pc] = logic.priorityStage(
        "PRIORITY_PC_MISALIGNED", permission, pc_misaligned);
    permission = after_pc;
    const auto [imem_event, after_imem] = logic.priorityStage(
        "PRIORITY_IMEM_FAULT", permission, imem_fault);
    permission = after_imem;
    const auto [decode_event, after_decode] = logic.priorityStage(
        "PRIORITY_DECODE_TRAP", permission, decode_trap_condition);
    permission = after_decode;
    const auto [halt_event, after_halt] = logic.priorityStage(
        "PRIORITY_HALT", permission, halt_request);
    permission = after_halt;
    const auto [data_misaligned_event, after_data_misaligned] = logic.priorityStage(
        "PRIORITY_DATA_MISALIGNED", permission, data_misaligned_condition);
    permission = after_data_misaligned;
    const auto [data_fault_event, after_data_fault] = logic.priorityStage(
        "PRIORITY_DATA_FAULT", permission, data_fault_condition);
    permission = after_data_fault;
    const auto [target_event, normal_completion] = logic.priorityStage(
        "PRIORITY_TARGET_MISALIGNED", permission, target_misaligned);

    logic.sink(normal_completion, getOutputPin("PC_WRITE"));
    const auto register_write = logic.logicalAnd(
        "REGISTER_WRITE_GATE", normal_completion, reg_write);
    logic.sink(register_write, getOutputPin("REGISTER_WRITE"));

    const auto trap_event = logic.orAll(
        "TRAP_EVENT",
        {pc_event, imem_event, decode_event, data_misaligned_event,
         data_fault_event, target_event});
    logic.sink(trap_event, builder.getInputPin<IOComponent>("TRAPPED_STATE", "WE"));
    logic.sink(halt_event, builder.getInputPin<IOComponent>("HALTED_STATE", "WE"));
    for (size_t bit = 0; bit < 4; ++bit) {
        logic.sink(trap_event, builder.getInputPin<IOComponent>(
            "TRAP_CAUSE_BIT_" + std::to_string(bit), "WE"));
    }

    const auto control_alignment_event = logic.logicalOr(
        "CONTROL_ALIGNMENT_EVENT", pc_event, target_event);
    logic.sink(legal, builder.getInputPin<Mux2to1_4bit>("DECODE_CAUSE_MUX", "SEL"));
    logic.sink(mem_write, builder.getInputPin<Mux2to1_4bit>("DATA_MISALIGNED_CAUSE_MUX", "SEL"));
    logic.sink(mem_write, builder.getInputPin<Mux2to1_4bit>("DATA_ACCESS_CAUSE_MUX", "SEL"));
    logic.sink(control_alignment_event, builder.getInputPin<Mux2to1_4bit>("CAUSE_CONTROL_MUX", "SEL"));
    logic.sink(imem_event, builder.getInputPin<Mux2to1_4bit>("CAUSE_IMEM_MUX", "SEL"));
    logic.sink(decode_event, builder.getInputPin<Mux2to1_4bit>("CAUSE_DECODE_MUX", "SEL"));
    logic.sink(data_misaligned_event, builder.getInputPin<Mux2to1_4bit>("CAUSE_DATA_MISALIGNED_MUX", "SEL"));
    logic.sink(data_fault_event, builder.getInputPin<Mux2to1_4bit>("CAUSE_DATA_ACCESS_MUX", "SEL"));

    builder.addNewWire<4>(
        "CAUSE_ILLEGAL_to_decode_mux",
        builder.getOutputPin<ConstantValue<4>, 4>("CAUSE_ILLEGAL", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("DECODE_CAUSE_MUX", "A")});
    builder.addNewWire<4>(
        "DECODE_CAUSE_to_mux",
        getInputPin<4>("DECODE_TRAP_CAUSE"),
        {builder.getInputPin<Mux2to1_4bit, 4>("DECODE_CAUSE_MUX", "B")});
    builder.addNewWire<4>(
        "LOAD_MISALIGNED_to_data_mux",
        builder.getOutputPin<ConstantValue<4>, 4>("CAUSE_LOAD_MISALIGNED", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("DATA_MISALIGNED_CAUSE_MUX", "A")});
    builder.addNewWire<4>(
        "STORE_MISALIGNED_to_data_mux",
        builder.getOutputPin<ConstantValue<4>, 4>("CAUSE_STORE_MISALIGNED", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("DATA_MISALIGNED_CAUSE_MUX", "B")});
    builder.addNewWire<4>(
        "LOAD_ACCESS_to_data_mux",
        builder.getOutputPin<ConstantValue<4>, 4>("CAUSE_LOAD_ACCESS", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("DATA_ACCESS_CAUSE_MUX", "A")});
    builder.addNewWire<4>(
        "STORE_ACCESS_to_data_mux",
        builder.getOutputPin<ConstantValue<4>, 4>("CAUSE_STORE_ACCESS", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("DATA_ACCESS_CAUSE_MUX", "B")});

    builder.addNewWire<4>(
        "CAUSE_NONE_to_control_mux",
        builder.getOutputPin<ConstantValue<4>, 4>("CAUSE_NONE", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("CAUSE_CONTROL_MUX", "A")});
    builder.addNewWire<4>(
        "CAUSE_CONTROL_to_control_mux",
        builder.getOutputPin<ConstantValue<4>, 4>("CAUSE_INSTRUCTION_MISALIGNED", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("CAUSE_CONTROL_MUX", "B")});
    builder.addNewWire<4>(
        "control_cause_to_imem_mux",
        builder.getOutputPin<Mux2to1_4bit, 4>("CAUSE_CONTROL_MUX", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("CAUSE_IMEM_MUX", "A")});
    builder.addNewWire<4>(
        "CAUSE_IMEM_to_imem_mux",
        builder.getOutputPin<ConstantValue<4>, 4>("CAUSE_INSTRUCTION_ACCESS", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("CAUSE_IMEM_MUX", "B")});
    builder.addNewWire<4>(
        "imem_cause_to_decode_mux",
        builder.getOutputPin<Mux2to1_4bit, 4>("CAUSE_IMEM_MUX", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("CAUSE_DECODE_MUX", "A")});
    builder.addNewWire<4>(
        "effective_decode_cause_to_priority_mux",
        builder.getOutputPin<Mux2to1_4bit, 4>("DECODE_CAUSE_MUX", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("CAUSE_DECODE_MUX", "B")});
    builder.addNewWire<4>(
        "decode_cause_to_data_misaligned_mux",
        builder.getOutputPin<Mux2to1_4bit, 4>("CAUSE_DECODE_MUX", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("CAUSE_DATA_MISALIGNED_MUX", "A")});
    builder.addNewWire<4>(
        "selected_data_misaligned_cause",
        builder.getOutputPin<Mux2to1_4bit, 4>("DATA_MISALIGNED_CAUSE_MUX", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("CAUSE_DATA_MISALIGNED_MUX", "B")});
    builder.addNewWire<4>(
        "data_misaligned_cause_to_access_mux",
        builder.getOutputPin<Mux2to1_4bit, 4>("CAUSE_DATA_MISALIGNED_MUX", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("CAUSE_DATA_ACCESS_MUX", "A")});
    builder.addNewWire<4>(
        "selected_data_access_cause",
        builder.getOutputPin<Mux2to1_4bit, 4>("DATA_ACCESS_CAUSE_MUX", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>("CAUSE_DATA_ACCESS_MUX", "B")});
    builder.addNewWire<4>(
        "final_cause_to_state",
        builder.getOutputPin<Mux2to1_4bit, 4>("CAUSE_DATA_ACCESS_MUX", "OUT"),
        {builder.getInputPin<BitSplitter<4>, 4>("CAUSE_D_SPLIT", "IN")});

    logic.materialize();
}
