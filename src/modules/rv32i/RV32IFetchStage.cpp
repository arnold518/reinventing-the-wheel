#include "modules/rv32i/RV32IPipelineStages.hpp"
#include <string>
#include <vector>
#include <stdexcept>

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/IncrementBy4.hpp"
#include "modules/memory/Register.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "RV32IPipelineStagesInternal.hpp"

using namespace rv32i::pipeline_stages;

RV32IFetchStage::RV32IFetchStage(std::string name, bool external_next_pc)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IFetchStage.pinInitializer()),
      external_next_pc_(external_next_pc) {}

void RV32IFetchStage::buildInternals(ComponentBuilder& builder) {
    using PinPtr = std::shared_ptr<PinBase>;
    // One named net per call, created in this order.
    const auto wire = [&](const std::string& name, const PinPtr& source,
                          const std::vector<PinPtr>& sinks) {
        if (!source) throw std::logic_error("RV32IFetchStage net has no source: " + name);
        builder.addNewWireDynamic(name, source->getWidth(), source, sinks);
    };
    const auto pc = builder.add(
        circuit::families::registerWord(32), "PC_STATE");
    const auto alignment_bits=builder.addNewComponent<Rewire>("ALIGNMENT_BITS",
        std::vector<Rewire::WireSpec>{{"ADDRESS",32}},
        std::vector<Rewire::WireSpec>{{"BIT_0",1},{"BIT_1",1}},
        std::vector<Rewire::BitMap>{{"ADDRESS",0,"BIT_0",0},{"ADDRESS",1,"BIT_1",0}});
    const auto alignment=builder.addNewComponent<ORGate>("ALIGNMENT");
    if (!external_next_pc_) {
        builder.addNewComponent<IncrementBy4>("PC_PLUS_4");
        builder.addNewComponent<Mux2to1_32bit>("NEXT_PC_MUX");
    } else {
        builder.addNewComponent<Rewire>("NEXT_PC_BRIDGE",
            std::vector<Rewire::WireSpec>{{"IN",32}},
            std::vector<Rewire::WireSpec>{{"OUT",32}},
            identity_mapping("IN",0,32,"OUT"));
    }
    builder.addNewComponent<Mux2to1_32bit>("PC_OUTPUT_MUX");
    builder.addNewComponent<Mux2to1_32bit>("INSTRUCTION_OUTPUT_MUX");
    builder.addNewComponent<Mux2to1_32bit>("STATUS_OUTPUT_MUX");
    builder.addNewComponent<ConstantValue<32>>("ZERO", 0);
    addPassThrough(builder, "VALID_PASS", {{"VALID", 1}});
    addStatusPack(
        builder,
        "STATUS_PACK",
        {"PC_MISALIGNED", "INSTRUCTION_FAULT"});

    wire("CLK", getInputPin("CLK"), {pc->getInputPin("CLK")});
    wire("RST", getInputPin("RST"), {pc->getInputPin("RST")});
    wire("PC_WRITE", getInputPin("PC_WRITE"), {pc->getInputPin("WE")});
    if (!external_next_pc_)
        wire("PC_REDIRECT", getInputPin("PC_REDIRECT"),
            {builder.getInputPin<Mux2to1_32bit>("NEXT_PC_MUX", "SEL")});
    wire("FETCH_VALID", getInputPin("FETCH_VALID"),
        {builder.getInputPin<Mux2to1_32bit>("PC_OUTPUT_MUX", "SEL"),
         builder.getInputPin<Mux2to1_32bit>("INSTRUCTION_OUTPUT_MUX", "SEL"),
         builder.getInputPin<Mux2to1_32bit>("STATUS_OUTPUT_MUX", "SEL"),
         builder.getInputPin<Rewire>("VALID_PASS", "IN_VALID")});
    wire("REDIRECT_PC", getInputPin<32>("REDIRECT_PC"), external_next_pc_ ? std::vector<PinPtr>{builder.getInputPin<Rewire,32>("NEXT_PC_BRIDGE",
        "IN")} : std::vector<PinPtr>{builder.getInputPin<Mux2to1_32bit,32>("NEXT_PC_MUX", "B")});
    wire("IMEM_READ_DATA", getInputPin<32>("IMEM_READ_DATA"),
        {builder.getInputPin<Mux2to1_32bit, 32>("INSTRUCTION_OUTPUT_MUX", "B")});
    wire("IMEM_FAULT", getInputPin("IMEM_FAULT"),
        {builder.getInputPin<Rewire>("STATUS_PACK", "INSTRUCTION_FAULT")});
    wire("ALIGNMENT_BIT_0",alignment_bits->getOutputPin("BIT_0"),{alignment->getInputPin("A")});
    wire("ALIGNMENT_BIT_1",alignment_bits->getOutputPin("BIT_1"),{alignment->getInputPin("B")});
    wire("ZERO", builder.getOutputPin<ConstantValue<32>, 32>("ZERO", "OUT"),
        {builder.getInputPin<Mux2to1_32bit, 32>("PC_OUTPUT_MUX", "A"),
         builder.getInputPin<Mux2to1_32bit, 32>("INSTRUCTION_OUTPUT_MUX", "A"),
         builder.getInputPin<Mux2to1_32bit, 32>("STATUS_OUTPUT_MUX", "A")});
    wire("PC", pc->getOutputPin<32>("Q"),
        [&] {std::vector<PinPtr> sinks{alignment_bits->getInputPin<32>("ADDRESS"), builder.getInputPin<Mux2to1_32bit,32>("PC_OUTPUT_MUX", "B"), getOutputPin<32>("FETCH_PC"), getOutputPin<32>("IMEM_ADDR")}; if (!external_next_pc_) sinks.push_back(builder.getInputPin<IncrementBy4,32>("PC_PLUS_4", "A")); return sinks;}());
    if (!external_next_pc_)
        wire("PC_PLUS_4", builder.getOutputPin<IncrementBy4,32>("PC_PLUS_4", "Sum"),
            {builder.getInputPin<Mux2to1_32bit,32>("NEXT_PC_MUX", "A")});
    wire("NEXT_PC", external_next_pc_ ? builder.getOutputPin<Rewire,32>("NEXT_PC_BRIDGE", "OUT") : builder.getOutputPin<Mux2to1_32bit,32>("NEXT_PC_MUX", "OUT"),
        {pc->getInputPin<32>("D")});
    wire("MISALIGNED", alignment->getOutputPin("OUT"),
        {builder.getInputPin<Rewire>("STATUS_PACK", "PC_MISALIGNED")});
    wire("STATUS", builder.getOutputPin<Rewire, 32>("STATUS_PACK", "STATUS"),
        {builder.getInputPin<Mux2to1_32bit, 32>("STATUS_OUTPUT_MUX", "B")});
    wire("OUT_PC", builder.getOutputPin<Mux2to1_32bit, 32>("PC_OUTPUT_MUX", "OUT"),
        {getOutputPin<32>("OUT_PC")});
    wire("OUT_INSTRUCTION", builder.getOutputPin<Mux2to1_32bit, 32>("INSTRUCTION_OUTPUT_MUX", "OUT"),
        {getOutputPin<32>("OUT_INSTRUCTION")});
    wire("OUT_STATUS", builder.getOutputPin<Mux2to1_32bit, 32>("STATUS_OUTPUT_MUX", "OUT"),
        {getOutputPin<32>("OUT_FETCH_STATUS")});
    wire("OUT_VALID", builder.getOutputPin<Rewire>("VALID_PASS", "OUT_VALID"), {getOutputPin("OUT_VALID")});
}
