#include "modules/rv32i/RV32IPipelineControl.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/Constant.hpp"
#include "RV32ILogicNetwork.hpp"

namespace {
using rv32i::detail::LogicNetwork;
using rv32i::detail::NetId;
} // namespace

RV32IPipelineRetirementUnit::RV32IPipelineRetirementUnit(
    std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IPipelineRetirementUnit
              .pinInitializer()) {}

void RV32IPipelineRetirementUnit::buildInternals(
    ComponentBuilder& builder) {
    LogicNetwork logic(builder);
    const auto enable =
        logic.source("ENABLE", getInputPin("ENABLE"));
    const auto valid =
        logic.source("VALID", getInputPin("MEM_WB_VALID"));
    const auto pc_misaligned = logic.source(
        "PC_MISALIGNED", getInputPin("PC_MISALIGNED"));
    const auto instruction_fault = logic.source(
        "INSTRUCTION_FAULT",
        getInputPin("INSTRUCTION_FAULT"));
    const auto legal =
        logic.source("LEGAL", getInputPin("LEGAL"));
    const auto not_legal =
        logic.logicalNot("NOT_LEGAL", legal);
    const auto trap_request = logic.source(
        "TRAP_REQUEST", getInputPin("TRAP_REQUEST"));
    const auto data_misaligned = logic.source(
        "DATA_MISALIGNED", getInputPin("DATA_MISALIGNED"));
    const auto data_fault =
        logic.source("DATA_FAULT", getInputPin("DATA_FAULT"));
    const auto target_misaligned = logic.source(
        "TARGET_MISALIGNED",
        getInputPin("TARGET_MISALIGNED"));
    const auto halt_request = logic.source(
        "HALT_REQUEST", getInputPin("HALT_REQUEST"));
    const auto reg_write_request = logic.source(
        "REG_WRITE_REQUEST",
        getInputPin("REG_WRITE_REQUEST"));

    const auto trap_any = logic.orAll(
        "TRAP_ANY",
        {pc_misaligned,
         instruction_fault,
         not_legal,
         trap_request,
         data_misaligned,
         data_fault,
         target_misaligned});
    const auto commit =
        logic.logicalAnd("COMMIT", enable, valid);
    const auto trap_event =
        logic.logicalAnd("TRAP_EVENT", commit, trap_any);
    const auto not_trap =
        logic.logicalNot("NOT_TRAP", trap_any);
    const auto commit_without_trap = logic.logicalAnd(
        "COMMIT_WITHOUT_TRAP", commit, not_trap);
    const auto halt_event = logic.logicalAnd(
        "HALT_EVENT", commit_without_trap, halt_request);
    const auto not_halt =
        logic.logicalNot("NOT_HALT", halt_request);
    const auto normal_commit = logic.andAll(
        "NORMAL_COMMIT",
        {commit, not_trap, not_halt});
    const auto register_write = logic.logicalAnd(
        "REGISTER_WRITE",
        normal_commit,
        reg_write_request);

    logic.sink(commit, getOutputPin("COMMIT"));
    logic.sink(
        normal_commit, getOutputPin("NORMAL_COMMIT"));
    logic.sink(
        register_write, getOutputPin("REGISTER_WRITE"));
    logic.sink(halt_event, getOutputPin("HALT_EVENT"));
    logic.sink(trap_event, getOutputPin("TRAP_EVENT"));

    builder.addNewComponent<ConstantValue<4>>("CAUSE_NONE", 0);
    builder.addNewComponent<ConstantValue<4>>("CAUSE_ILLEGAL", 1);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_TARGET_MISALIGNED", 3);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_PC_MISALIGNED", 3);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_INSTRUCTION_FAULT", 4);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_LOAD_MISALIGNED", 5);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_LOAD_FAULT", 6);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_STORE_MISALIGNED", 7);
    builder.addNewComponent<ConstantValue<4>>(
        "CAUSE_STORE_FAULT", 8);
    builder.addNewComponent<Mux2to1_4bit>(
        "DATA_MISALIGNED_CAUSE");
    builder.addNewComponent<Mux2to1_4bit>(
        "DATA_FAULT_CAUSE");

    builder.addNewWire(
        "MEM_WRITE_to_data_cause_muxes",
        getInputPin("MEM_WRITE"),
        {builder.getInputPin<Mux2to1_4bit>(
             "DATA_MISALIGNED_CAUSE", "SEL"),
         builder.getInputPin<Mux2to1_4bit>(
             "DATA_FAULT_CAUSE", "SEL")});
    builder.addNewWire<4>(
        "LOAD_MISALIGNED_to_data_cause",
        builder.getOutputPin<ConstantValue<4>, 4>(
            "CAUSE_LOAD_MISALIGNED", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>(
            "DATA_MISALIGNED_CAUSE", "A")});
    builder.addNewWire<4>(
        "STORE_MISALIGNED_to_data_cause",
        builder.getOutputPin<ConstantValue<4>, 4>(
            "CAUSE_STORE_MISALIGNED", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>(
            "DATA_MISALIGNED_CAUSE", "B")});
    builder.addNewWire<4>(
        "LOAD_FAULT_to_data_cause",
        builder.getOutputPin<ConstantValue<4>, 4>(
            "CAUSE_LOAD_FAULT", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>(
            "DATA_FAULT_CAUSE", "A")});
    builder.addNewWire<4>(
        "STORE_FAULT_to_data_cause",
        builder.getOutputPin<ConstantValue<4>, 4>(
            "CAUSE_STORE_FAULT", "OUT"),
        {builder.getInputPin<Mux2to1_4bit, 4>(
            "DATA_FAULT_CAUSE", "B")});

    struct CauseStage {
        const char* name;
        NetId select;
        std::shared_ptr<Pin<4>> cause;
    };
    const std::array<CauseStage, 7> stages{{
        {"TARGET_CAUSE", target_misaligned,
         builder.getOutputPin<ConstantValue<4>, 4>(
             "CAUSE_TARGET_MISALIGNED", "OUT")},
        {"DATA_FAULT_SELECT", data_fault,
         builder.getOutputPin<Mux2to1_4bit, 4>(
             "DATA_FAULT_CAUSE", "OUT")},
        {"DATA_MISALIGNED_SELECT", data_misaligned,
         builder.getOutputPin<Mux2to1_4bit, 4>(
             "DATA_MISALIGNED_CAUSE", "OUT")},
        {"DECODE_TRAP_SELECT", trap_request,
         getInputPin<4>("DECODE_TRAP_CAUSE")},
        {"ILLEGAL_SELECT", not_legal,
         builder.getOutputPin<ConstantValue<4>, 4>(
             "CAUSE_ILLEGAL", "OUT")},
        {"INSTRUCTION_FAULT_SELECT", instruction_fault,
         builder.getOutputPin<ConstantValue<4>, 4>(
             "CAUSE_INSTRUCTION_FAULT", "OUT")},
        {"PC_MISALIGNED_SELECT", pc_misaligned,
         builder.getOutputPin<ConstantValue<4>, 4>(
             "CAUSE_PC_MISALIGNED", "OUT")},
    }};

    auto previous =
        builder.getOutputPin<ConstantValue<4>, 4>(
            "CAUSE_NONE", "OUT");
    for (const auto& stage : stages) {
        const auto mux_name =
            std::string(stage.name) + "_MUX";
        builder.addNewComponent<Mux2to1_4bit>(mux_name);
        builder.addNewWire<4>(
            mux_name + "_previous",
            previous,
            {builder.getInputPin<Mux2to1_4bit, 4>(
                mux_name, "A")});
        builder.addNewWire<4>(
            mux_name + "_cause",
            stage.cause,
            {builder.getInputPin<Mux2to1_4bit, 4>(
                mux_name, "B")});
        logic.sink(
            stage.select,
            builder.getInputPin<Mux2to1_4bit>(
                mux_name, "SEL"));
        previous =
            builder.getOutputPin<Mux2to1_4bit, 4>(
                mux_name, "OUT");
    }
    builder.addNewWire<4>(
        "TRAP_CAUSE_to_output",
        previous,
        {getOutputPin<4>("TRAP_CAUSE")});
    logic.materialize();
}
