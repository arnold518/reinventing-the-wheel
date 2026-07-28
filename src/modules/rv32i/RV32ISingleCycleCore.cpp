#include "modules/rv32i/RV32ISingleCycleCore.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IControlFlowUnit.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IExecutionControlStatusUnit.hpp"
#include "modules/rv32i/RV32IReferenceCore.hpp"
#include "modules/utility/Constant.hpp"
#include <stdexcept>
#include <string>
#include <utility>
#include <limits>

namespace {
void initializeCorePins(IOComponent* self) {
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
    self->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
    self->addPin("INSTRUCTION_ATTEMPT", PinType::OUTPUT);
    self->addPin<32>("IMEM_ADDR", PinType::OUTPUT);
    self->addPin("IMEM_READ_EN", PinType::OUTPUT);
    self->addPin<32>("DMEM_ADDR", PinType::OUTPUT);
    self->addPin<32>("DMEM_WRITE_DATA", PinType::OUTPUT);
    self->addPin("DMEM_READ_EN", PinType::OUTPUT);
    self->addPin("DMEM_WRITE_EN", PinType::OUTPUT);
    self->addPin<2>("DMEM_SIZE", PinType::OUTPUT);
    self->addPin("DMEM_SIGN_EXTEND", PinType::OUTPUT);
}
} // namespace

namespace circuit::families {
const ComponentFamily RV32ISingleCycleCore{
    "rv32i.core.educational-single-cycle",
    "RV32ISingleCycleCore",
    initializeCorePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32ISingleCycleCore>(
            context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IReferenceCore>(
            context, name);
    }};
}

RV32ISingleCycleCore::RV32ISingleCycleCore(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32ISingleCycleCore.pinInitializer()) {}

void RV32ISingleCycleCore::buildInternals(ComponentBuilder& builder) {
    control_flow_ = builder.add(
        circuit::families::RV32IControlFlow, "CONTROL_FLOW");
    decode_control_ = builder.add(
        circuit::families::RV32IDecodeControl, "DECODE_CONTROL");
    register_file_ = builder.add(
        circuit::families::RegisterFile32x32,
        "REGISTER_FILE", {}, {"register-state-view"});
    alu_ = builder.add(circuit::families::ALU32, "ALU");
    execution_status_ = builder.add(
        circuit::families::RV32IExecutionStatus, "EXECUTION_STATUS");

    builder.addNewComponent<Mux4to1_32bit>("ALU_A_MUX");
    builder.addNewComponent<Mux4to1_32bit>("ALU_B_MUX");
    builder.addNewComponent<Mux4to1_32bit>("WRITEBACK_MUX");
    builder.addNewComponent<ANDGate>("DMEM_READ_GATE");
    builder.addNewComponent<ANDGate>("DMEM_WRITE_GATE");
    builder.addNewComponent<ConstantValue<1>>("CONST_HIGH", 1);
    builder.addNewComponent<ConstantValue<32>>("CONST_ZERO32", 0);

    builder.addNewWire(
        "CLK_fanout",
        getInputPin("CLK"),
        {builder.getInputPin<IOComponent>("CONTROL_FLOW", "CLK"),
         builder.getInputPin<IOComponent>("REGISTER_FILE", "CLK"),
         builder.getInputPin<IOComponent>("EXECUTION_STATUS", "CLK")});
    builder.addNewWire(
        "RST_fanout",
        getInputPin("RST"),
        {builder.getInputPin<IOComponent>("CONTROL_FLOW", "RST"),
         builder.getInputPin<IOComponent>("REGISTER_FILE", "RST"),
         builder.getInputPin<IOComponent>("EXECUTION_STATUS", "RST")});
    builder.addNewWire(
        "ENABLE_to_status",
        getInputPin("ENABLE"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "ENABLE")});

    builder.addNewWire<32>(
        "INSTRUCTION_to_decode",
        getInputPin<32>("IMEM_READ_DATA"),
        {builder.getInputPin<IOComponent, 32>("DECODE_CONTROL", "INSTRUCTION")});
    builder.addNewWire(
        "IMEM_READY_to_status",
        getInputPin("IMEM_READY"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "IMEM_READY")});
    builder.addNewWire(
        "IMEM_FAULT_to_status",
        getInputPin("IMEM_FAULT"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "IMEM_FAULT")});
    builder.addNewWire(
        "DMEM_READY_to_status",
        getInputPin("DMEM_READY"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "DMEM_READY")});
    builder.addNewWire(
        "DMEM_FAULT_to_status",
        getInputPin("DMEM_FAULT"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "DMEM_FAULT")});

    builder.addNewWire<32>(
        "PC_fanout",
        builder.getOutputPin<IOComponent, 32>("CONTROL_FLOW", "PC"),
        {getOutputPin<32>("PC"),
         getOutputPin<32>("IMEM_ADDR"),
         builder.getInputPin<Mux4to1_32bit, 32>("ALU_A_MUX", "IN1")});
    builder.addNewWire<32>(
        "PC_PLUS_4_to_writeback",
        builder.getOutputPin<IOComponent, 32>("CONTROL_FLOW", "PC_PLUS_4"),
        {builder.getInputPin<Mux4to1_32bit, 32>("WRITEBACK_MUX", "IN3")});
    builder.addNewWire(
        "PC_MISALIGNED_to_status",
        builder.getOutputPin<IOComponent>("CONTROL_FLOW", "PC_MISALIGNED"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "PC_MISALIGNED")});
    builder.addNewWire(
        "TARGET_MISALIGNED_to_status",
        builder.getOutputPin<IOComponent>("CONTROL_FLOW", "TARGET_MISALIGNED"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "TARGET_MISALIGNED")});

    builder.addNewWire<5>(
        "RS1_ADDR_to_register_file",
        builder.getOutputPin<IOComponent, 5>("DECODE_CONTROL", "RS1_ADDR"),
        {builder.getInputPin<IOComponent, 5>("REGISTER_FILE", "RS1_ADDR")});
    builder.addNewWire<5>(
        "RS2_ADDR_to_register_file",
        builder.getOutputPin<IOComponent, 5>("DECODE_CONTROL", "RS2_ADDR"),
        {builder.getInputPin<IOComponent, 5>("REGISTER_FILE", "RS2_ADDR")});
    builder.addNewWire<5>(
        "RD_ADDR_to_register_file",
        builder.getOutputPin<IOComponent, 5>("DECODE_CONTROL", "RD_ADDR"),
        {builder.getInputPin<IOComponent, 5>("REGISTER_FILE", "RD_ADDR")});
    builder.addNewWire<32>(
        "IMM_fanout",
        builder.getOutputPin<IOComponent, 32>("DECODE_CONTROL", "IMM"),
        {builder.getInputPin<Mux4to1_32bit, 32>("ALU_B_MUX", "IN1"),
         builder.getInputPin<IOComponent, 32>("CONTROL_FLOW", "IMM")});
    builder.addNewWire<5>(
        "ALU_OP_to_alu",
        builder.getOutputPin<IOComponent, 5>("DECODE_CONTROL", "ALU_OP"),
        {builder.getInputPin<IOComponent, 5>("ALU", "OP")});
    builder.addNewWire<2>(
        "ALU_A_SEL_to_mux",
        builder.getOutputPin<IOComponent, 2>("DECODE_CONTROL", "ALU_A_SEL"),
        {builder.getInputPin<Mux4to1_32bit, 2>("ALU_A_MUX", "SEL")});
    builder.addNewWire<2>(
        "ALU_B_SEL_to_mux",
        builder.getOutputPin<IOComponent, 2>("DECODE_CONTROL", "ALU_B_SEL"),
        {builder.getInputPin<Mux4to1_32bit, 2>("ALU_B_MUX", "SEL")});

    builder.addNewWire(
        "LEGAL_to_status",
        builder.getOutputPin<IOComponent>("DECODE_CONTROL", "LEGAL"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "LEGAL")});
    builder.addNewWire(
        "RAW_REG_WRITE_to_status",
        builder.getOutputPin<IOComponent>("DECODE_CONTROL", "REG_WRITE"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "REG_WRITE")});
    builder.addNewWire(
        "RAW_MEM_READ_fanout",
        builder.getOutputPin<IOComponent>("DECODE_CONTROL", "MEM_READ"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "MEM_READ"),
         builder.getInputPin<ANDGate>("DMEM_READ_GATE", "B")});
    builder.addNewWire(
        "RAW_MEM_WRITE_fanout",
        builder.getOutputPin<IOComponent>("DECODE_CONTROL", "MEM_WRITE"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "MEM_WRITE"),
         builder.getInputPin<ANDGate>("DMEM_WRITE_GATE", "B")});
    builder.addNewWire<2>(
        "WRITEBACK_SEL_to_mux",
        builder.getOutputPin<IOComponent, 2>("DECODE_CONTROL", "WRITEBACK_SEL"),
        {builder.getInputPin<Mux4to1_32bit, 2>("WRITEBACK_MUX", "SEL")});
    builder.addNewWire<2>(
        "MEM_SIZE_fanout",
        builder.getOutputPin<IOComponent, 2>("DECODE_CONTROL", "MEM_SIZE"),
        {getOutputPin<2>("DMEM_SIZE"),
         builder.getInputPin<IOComponent, 2>("EXECUTION_STATUS", "MEM_SIZE")});
    builder.addNewWire(
        "LOAD_SIGN_EXTEND_to_dmem",
        builder.getOutputPin<IOComponent>("DECODE_CONTROL", "LOAD_SIGN_EXTEND"),
        {getOutputPin("DMEM_SIGN_EXTEND")});
    builder.addNewWire<3>(
        "BRANCH_TYPE_to_control_flow",
        builder.getOutputPin<IOComponent, 3>("DECODE_CONTROL", "BRANCH_TYPE"),
        {builder.getInputPin<IOComponent, 3>("CONTROL_FLOW", "BRANCH_TYPE")});
    builder.addNewWire<2>(
        "JUMP_TYPE_to_control_flow",
        builder.getOutputPin<IOComponent, 2>("DECODE_CONTROL", "JUMP_TYPE"),
        {builder.getInputPin<IOComponent, 2>("CONTROL_FLOW", "JUMP_TYPE")});
    builder.addNewWire(
        "HALT_REQUEST_to_status",
        builder.getOutputPin<IOComponent>("DECODE_CONTROL", "HALT_REQUEST"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "HALT_REQUEST")});
    builder.addNewWire(
        "TRAP_REQUEST_to_status",
        builder.getOutputPin<IOComponent>("DECODE_CONTROL", "TRAP_REQUEST"),
        {builder.getInputPin<IOComponent>("EXECUTION_STATUS", "TRAP_REQUEST")});
    builder.addNewWire<4>(
        "DECODE_TRAP_CAUSE_to_status",
        builder.getOutputPin<IOComponent, 4>("DECODE_CONTROL", "DECODE_TRAP_CAUSE"),
        {builder.getInputPin<IOComponent, 4>("EXECUTION_STATUS", "DECODE_TRAP_CAUSE")});

    builder.addNewWire<32>(
        "RS1_DATA_fanout",
        builder.getOutputPin<IOComponent, 32>("REGISTER_FILE", "RS1_DATA"),
        {builder.getInputPin<Mux4to1_32bit, 32>("ALU_A_MUX", "IN0"),
         builder.getInputPin<IOComponent, 32>("CONTROL_FLOW", "RS1_VALUE")});
    builder.addNewWire<32>(
        "RS2_DATA_fanout",
        builder.getOutputPin<IOComponent, 32>("REGISTER_FILE", "RS2_DATA"),
        {builder.getInputPin<Mux4to1_32bit, 32>("ALU_B_MUX", "IN0"),
         getOutputPin<32>("DMEM_WRITE_DATA")});
    builder.addNewWire<32>(
        "WRITEBACK_DATA_to_register_file",
        builder.getOutputPin<Mux4to1_32bit, 32>("WRITEBACK_MUX", "OUT"),
        {builder.getInputPin<IOComponent, 32>("REGISTER_FILE", "WRITE_DATA")});

    builder.addNewWire<32>(
        "CONST_ZERO32_fanout",
        builder.getOutputPin<ConstantValue<32>, 32>("CONST_ZERO32", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>("ALU_A_MUX", "IN2"),
         builder.getInputPin<Mux4to1_32bit, 32>("ALU_A_MUX", "IN3"),
         builder.getInputPin<Mux4to1_32bit, 32>("ALU_B_MUX", "IN2"),
         builder.getInputPin<Mux4to1_32bit, 32>("ALU_B_MUX", "IN3"),
         builder.getInputPin<Mux4to1_32bit, 32>("WRITEBACK_MUX", "IN0")});
    builder.addNewWire(
        "CONST_HIGH_to_imem_read",
        builder.getOutputPin<ConstantValue<1>>("CONST_HIGH", "OUT"),
        {getOutputPin("IMEM_READ_EN")});

    builder.addNewWire<32>(
        "ALU_A_to_alu",
        builder.getOutputPin<Mux4to1_32bit, 32>("ALU_A_MUX", "OUT"),
        {builder.getInputPin<IOComponent, 32>("ALU", "A")});
    builder.addNewWire<32>(
        "ALU_B_to_alu",
        builder.getOutputPin<Mux4to1_32bit, 32>("ALU_B_MUX", "OUT"),
        {builder.getInputPin<IOComponent, 32>("ALU", "B")});
    builder.addNewWire<32>(
        "ALU_RESULT_fanout",
        builder.getOutputPin<IOComponent, 32>("ALU", "OUT"),
        {builder.getInputPin<Mux4to1_32bit, 32>("WRITEBACK_MUX", "IN1"),
         builder.getInputPin<IOComponent, 32>("EXECUTION_STATUS", "ALU_ADDRESS"),
         getOutputPin<32>("DMEM_ADDR")});
    builder.addNewWire(
        "ALU_EQ_to_control_flow",
        builder.getOutputPin<IOComponent>("ALU", "EQ"),
        {builder.getInputPin<IOComponent>("CONTROL_FLOW", "EQ")});
    builder.addNewWire(
        "ALU_LT_SIGNED_to_control_flow",
        builder.getOutputPin<IOComponent>("ALU", "LT_SIGNED"),
        {builder.getInputPin<IOComponent>("CONTROL_FLOW", "LT_SIGNED")});
    builder.addNewWire(
        "ALU_LT_UNSIGNED_to_control_flow",
        builder.getOutputPin<IOComponent>("ALU", "LT_UNSIGNED"),
        {builder.getInputPin<IOComponent>("CONTROL_FLOW", "LT_UNSIGNED")});
    builder.addNewWire<32>(
        "DMEM_READ_DATA_to_writeback",
        getInputPin<32>("DMEM_READ_DATA"),
        {builder.getInputPin<Mux4to1_32bit, 32>("WRITEBACK_MUX", "IN2")});

    builder.addNewWire(
        "PC_WRITE_to_control_flow",
        builder.getOutputPin<IOComponent>("EXECUTION_STATUS", "PC_WRITE"),
        {builder.getInputPin<IOComponent>("CONTROL_FLOW", "PC_WRITE")});
    builder.addNewWire(
        "REGISTER_WRITE_to_register_file",
        builder.getOutputPin<IOComponent>("EXECUTION_STATUS", "REGISTER_WRITE"),
        {builder.getInputPin<IOComponent>("REGISTER_FILE", "REG_WRITE")});
    builder.addNewWire(
        "MEMORY_REQUEST_fanout",
        builder.getOutputPin<IOComponent>("EXECUTION_STATUS", "MEMORY_REQUEST_ACTIVE"),
        {builder.getInputPin<ANDGate>("DMEM_READ_GATE", "A"),
         builder.getInputPin<ANDGate>("DMEM_WRITE_GATE", "A")});
    builder.addNewWire(
        "HALTED_to_output",
        builder.getOutputPin<IOComponent>("EXECUTION_STATUS", "HALTED"),
        {getOutputPin("HALTED")});
    builder.addNewWire(
        "TRAPPED_to_output",
        builder.getOutputPin<IOComponent>("EXECUTION_STATUS", "TRAPPED"),
        {getOutputPin("TRAPPED")});
    builder.addNewWire<4>(
        "TRAP_CAUSE_to_output",
        builder.getOutputPin<IOComponent, 4>("EXECUTION_STATUS", "TRAP_CAUSE"),
        {getOutputPin<4>("TRAP_CAUSE")});
    builder.addNewWire(
        "INSTRUCTION_ATTEMPT_to_output",
        builder.getOutputPin<IOComponent>("EXECUTION_STATUS", "INSTRUCTION_ATTEMPT"),
        {getOutputPin("INSTRUCTION_ATTEMPT")});

    builder.addNewWire(
        "DMEM_READ_GATE_to_output",
        builder.getOutputPin<ANDGate>("DMEM_READ_GATE", "OUT"),
        {getOutputPin("DMEM_READ_EN")});
    builder.addNewWire(
        "DMEM_WRITE_GATE_to_output",
        builder.getOutputPin<ANDGate>("DMEM_WRITE_GATE", "OUT"),
        {getOutputPin("DMEM_WRITE_EN")});
}

rv32i::RV32IArchitecturalState
RV32ISingleCycleCore::snapshotArchitecturalState() const {
    if (!control_flow_ || !register_file_ || !execution_status_) {
        throw std::logic_error("RV32ISingleCycleCore is not built");
    }

    rv32i::RV32IArchitecturalState state;
    state.pc =
        control_flow_->getOutputPin<32>("PC")->getValueAsVector();
    const auto register_view = std::dynamic_pointer_cast<RegisterStateView>(register_file_);
    if (!register_view) {
        throw std::logic_error("Selected register file lacks register-state-view capability");
    }
    const auto registers = register_view->getRegisterStateAtTime(
        std::numeric_limits<size_t>::max());
    for (size_t index = 0;
         index < state.x.size() && index < registers.size();
         ++index) {
        state.x[index] = registers[index];
    }
    state.halted =
        execution_status_->getOutputPin("HALTED")->getValue();
    state.trapped =
        execution_status_->getOutputPin("TRAPPED")->getValue();
    state.trap_cause =
        execution_status_->getOutputPin<4>(
            "TRAP_CAUSE")->getValueAsVector();
    return state;
}
