#include "modules/rv32i/RV32IPipelineStages.hpp"

#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/IncrementBy4.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/memory/MemoryController.hpp"
#include "modules/memory/Register.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPipelineEncoding.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "RV32IPipelineStagesInternal.hpp"

using namespace rv32i::pipeline_stages;

RV32IWritebackStage::RV32IWritebackStage(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IWritebackStage.pinInitializer()) {}

void RV32IWritebackStage::buildInternals(ComponentBuilder& builder) {
    using circuit::memory_detail::StructuralWiring;
    StructuralWiring w(builder);
    auto retirement = builder.add(circuit::families::RV32IPipelineRetirementUnit, "RETIREMENT");
    auto select = builder.addNewComponent<Mux4to1_32bit>("WRITEBACK_MUX");
    auto zero = builder.addNewComponent<ConstantValue<32>>("ZERO32", 0);

    addControlUnpack(builder, "CONTROL_UNPACK");
    addAddressUnpack(builder, "ADDRESSES_UNPACK");
    addStatusUnpack(builder, "FETCH_STATUS_UNPACK", {"PC_MISALIGNED", "INSTRUCTION_FAULT"});
    addStatusUnpack(builder, "EXECUTION_STATUS_UNPACK", {"TARGET_MISALIGNED"});
    addStatusUnpack(builder, "MEMORY_STATUS_UNPACK", {"DATA_MISALIGNED", "DATA_FAULT"});
    const auto control = builder.getComponent<Rewire>("CONTROL_UNPACK");
    const auto addresses = builder.getComponent<Rewire>("ADDRESSES_UNPACK");
    const auto fetch = builder.getComponent<Rewire>("FETCH_STATUS_UNPACK");
    const auto execute = builder.getComponent<Rewire>("EXECUTION_STATUS_UNPACK");
    const auto memory = builder.getComponent<Rewire>("MEMORY_STATUS_UNPACK");
    addPassThrough(builder, "VALID_PASS", {{"VALID", 1}});
    const auto valid = builder.getComponent<Rewire>("VALID_PASS");

    w.connect(getInputPin<32>("IN_CONTROL"), control->getInputPin<32>("CONTROL"));
    w.connect(getInputPin<32>("IN_REGISTER_ADDRESSES"), addresses->getInputPin<32>("ADDRESSES"));
    for (auto [name, unpack] : std::vector<std::pair<std::string, std::shared_ptr<Rewire>>>{
            {"IN_FETCH_STATUS", fetch}, {"IN_EXECUTION_STATUS", execute}, {"IN_MEMORY_STATUS", memory}})
        w.connect(getInputPin<32>(name), unpack->getInputPin<32>("STATUS"));
    w.connect(zero->getOutputPin<32>("OUT"), select->getInputPin<32>("IN0"));
    w.connect(getInputPin<32>("IN_ALU_RESULT"), select->getInputPin<32>("IN1"));
    w.connect(getInputPin<32>("IN_MEMORY_DATA"), select->getInputPin<32>("IN2"));
    w.connect(getInputPin<32>("IN_PC_PLUS_4"), select->getInputPin<32>("IN3"));
    w.connect(control->getOutputPin<2>("WRITEBACK_SEL"), select->getInputPin<2>("SEL"));
    w.connect(select->getOutputPin<32>("OUT"), getOutputPin<32>("WB_VALUE"));
    w.connect(addresses->getOutputPin<5>("RD"), getOutputPin<5>("WB_RD"));
    w.connect(getInputPin("IN_VALID"), valid->getInputPin("IN_VALID"));
    w.connect(valid->getOutputPin("OUT_VALID"), getOutputPin("WB_VALID"));
    w.connect(control->getOutputPin("REG_WRITE"), getOutputPin("WB_REG_WRITE"));

    w.connect(getInputPin("COMMIT_ENABLE"), retirement->getInputPin("ENABLE"));
    w.connect(getInputPin("IN_VALID"), retirement->getInputPin("MEM_WB_VALID"));
    for (const auto* name : {"LEGAL", "TRAP_REQUEST", "HALT_REQUEST", "MEM_READ", "MEM_WRITE"})
        w.connect(control->getOutputPin(name), retirement->getInputPin(name));
    w.connect(control->getOutputPin("REG_WRITE"), retirement->getInputPin("REG_WRITE_REQUEST"));
    w.connect(control->getOutputPin<4>("DECODE_TRAP_CAUSE"), retirement->getInputPin<4>("DECODE_TRAP_CAUSE"));
    for (const auto* name : {"PC_MISALIGNED", "INSTRUCTION_FAULT"})
        w.connect(fetch->getOutputPin(name), retirement->getInputPin(name));
    w.connect(execute->getOutputPin("TARGET_MISALIGNED"), retirement->getInputPin("TARGET_MISALIGNED"));
    for (const auto* name : {"DATA_MISALIGNED", "DATA_FAULT"})
        w.connect(memory->getOutputPin(name), retirement->getInputPin(name));

    // TERMINAL is ungated: the coordinator must see the pending terminal
    // packet even when COMMIT_ENABLE is low.
    const auto illegal = w.invert("NOT_LEGAL", control->getOutputPin("LEGAL"));
    const auto reason = w.reduce("TERMINAL_REASON", {
        fetch->getOutputPin("PC_MISALIGNED"), fetch->getOutputPin("INSTRUCTION_FAULT"),
        illegal, control->getOutputPin("TRAP_REQUEST"), control->getOutputPin("HALT_REQUEST"),
        execute->getOutputPin("TARGET_MISALIGNED"), memory->getOutputPin("DATA_MISALIGNED"),
        memory->getOutputPin("DATA_FAULT")}, false);
    const auto terminal = w.both("TERMINAL_VALID", getInputPin("IN_VALID"), reason);
    const auto retire_terminal = w.either("RETIRE_TERMINAL",
        retirement->getOutputPin("HALT_EVENT"), retirement->getOutputPin("TRAP_EVENT"));
    w.connect(terminal, getOutputPin("TERMINAL"));
    w.connect(retire_terminal, getOutputPin("RETIRE_TERMINAL"));
    w.connect(retirement->getOutputPin("COMMIT"), getOutputPin("COMMIT_VALID"));
    w.connect(retirement->getOutputPin("REGISTER_WRITE"), getOutputPin("REGISTER_WRITE"));

    for (const auto* name : {"HALT_EVENT", "TRAP_EVENT"})
        w.connect(retirement->getOutputPin(name), getOutputPin(name));
    w.connect(retirement->getOutputPin<4>("TRAP_CAUSE"), getOutputPin<4>("TRAP_CAUSE"));
    w.finish();
}

RV32IPipelineExecutionStatus::RV32IPipelineExecutionStatus(std::string name)
    : IOComponent(std::move(name),
                  circuit::families::RV32IPipelineExecutionStatus.pinInitializer()) {}

void RV32IPipelineExecutionStatus::buildInternals(ComponentBuilder& builder) {
    using circuit::memory_detail::StructuralWiring;
    StructuralWiring w(builder);
    const auto high = w.constant("HIGH", true);
    const auto halted = builder.add(circuit::families::FlipFlopCell, "HALTED_STATE");
    const auto trapped = builder.add(circuit::families::FlipFlopCell, "TRAPPED_STATE");
    const auto cause = builder.add(circuit::families::registerWord(4), "TRAP_CAUSE_WORD");
    for (auto state : {halted, trapped})
        for (const auto* pin : {"CLK", "RST"}) w.connect(getInputPin(pin), state->getInputPin(pin));
    w.connect(high, halted->getInputPin("D"));
    w.connect(high, trapped->getInputPin("D"));
    w.connect(getInputPin("HALT_EVENT"), halted->getInputPin("WE"));
    w.connect(getInputPin("TRAP_EVENT"), trapped->getInputPin("WE"));
    w.connect(halted->getOutputPin("Q"), getOutputPin("HALTED"));
    w.connect(trapped->getOutputPin("Q"), getOutputPin("TRAPPED"));
    w.connect(getInputPin<4>("EVENT_TRAP_CAUSE"), cause->getInputPin<4>("D"));
    for (const auto* pin : {"CLK", "RST"}) w.connect(getInputPin(pin), cause->getInputPin(pin));
    w.connect(getInputPin("TRAP_EVENT"), cause->getInputPin("WE"));
    w.connect(cause->getOutputPin<4>("Q"), getOutputPin<4>("TRAP_CAUSE"));
    w.finish();
}
