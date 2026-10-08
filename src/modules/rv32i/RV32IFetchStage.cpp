#include "modules/rv32i/RV32IPipelineStages.hpp"
#include <string>
#include <vector>
#include <stdexcept>

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/IncrementBy4.hpp"
#include "modules/storage/Register.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "RV32IPipelineStagesInternal.hpp"

using namespace rv32i::pipeline_stages;

RV32IFetchStage::RV32IFetchStage(std::string name, bool external_next_pc)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IFetchStage.pinInitializer()),
      external_next_pc_(external_next_pc) {}

void RV32IFetchStage::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    using PinPtr = std::shared_ptr<PinBase>;
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
    }
    builder.addNewComponent<Mux2to1_32bit>("PC_OUTPUT_MUX");
    builder.addNewComponent<Mux2to1_32bit>("INSTRUCTION_OUTPUT_MUX");
    builder.addNewComponent<Mux2to1_32bit>("STATUS_OUTPUT_MUX");
    builder.addNewComponent<ConstantValue<32>>("ZERO", 0);
    addStatusPack(
        builder,
        "STATUS_PACK",
        {"PC_MISALIGNED", "INSTRUCTION_FAULT"});

    w.connect(getInputPin("CLK"), {pc->getInputPin("CLK")});
    w.connect(getInputPin("RST"), {pc->getInputPin("RST")});
    w.connect(getInputPin("PC_WRITE"), {pc->getInputPin("WE")});
    if (!external_next_pc_)
        w.connect(getInputPin("PC_REDIRECT"),
            {builder.getInputPin<Mux2to1_32bit>("NEXT_PC_MUX", "SEL")});
    w.connect(getInputPin("FETCH_VALID"),
        {builder.getInputPin<Mux2to1_32bit>("PC_OUTPUT_MUX", "SEL"),
         builder.getInputPin<Mux2to1_32bit>("INSTRUCTION_OUTPUT_MUX", "SEL"),
         builder.getInputPin<Mux2to1_32bit>("STATUS_OUTPUT_MUX", "SEL"),
         getOutputPin("OUT_VALID")});
    w.connect(getInputPin<32>("REDIRECT_PC"), external_next_pc_ ? std::vector<PinPtr>{pc->getInputPin<32>("D")} : std::vector<PinPtr>{builder.getInputPin<Mux2to1_32bit,32>("NEXT_PC_MUX", "B")});
    w.connect(getInputPin<32>("IMEM_READ_DATA"),
        {builder.getInputPin<Mux2to1_32bit, 32>("INSTRUCTION_OUTPUT_MUX", "B")});
    w.connect(getInputPin("IMEM_FAULT"),
        {builder.getInputPin<Rewire>("STATUS_PACK", "INSTRUCTION_FAULT")});
    w.connect(alignment_bits->getOutputPin("BIT_0"),{alignment->getInputPin("A")});
    w.connect(alignment_bits->getOutputPin("BIT_1"),{alignment->getInputPin("B")});
    w.connect(builder.getOutputPin<ConstantValue<32>, 32>("ZERO", "OUT"),
        {builder.getInputPin<Mux2to1_32bit, 32>("PC_OUTPUT_MUX", "A"),
         builder.getInputPin<Mux2to1_32bit, 32>("INSTRUCTION_OUTPUT_MUX", "A"),
         builder.getInputPin<Mux2to1_32bit, 32>("STATUS_OUTPUT_MUX", "A")});
    w.connect(pc->getOutputPin<32>("Q"),
        [&] {std::vector<PinPtr> sinks{alignment_bits->getInputPin<32>("ADDRESS"), builder.getInputPin<Mux2to1_32bit,32>("PC_OUTPUT_MUX", "B"), getOutputPin<32>("FETCH_PC"), getOutputPin<32>("IMEM_ADDR")}; if (!external_next_pc_) sinks.push_back(builder.getInputPin<IncrementBy4,32>("PC_PLUS_4", "A")); return sinks;}());
    if (!external_next_pc_)
        w.connect(builder.getOutputPin<IncrementBy4,32>("PC_PLUS_4", "Sum"),
            {builder.getInputPin<Mux2to1_32bit,32>("NEXT_PC_MUX", "A")});
    if (!external_next_pc_)
        w.connect(builder.getOutputPin<Mux2to1_32bit,32>("NEXT_PC_MUX", "OUT"),
            {pc->getInputPin<32>("D")});
    w.connect(alignment->getOutputPin("OUT"),
        {builder.getInputPin<Rewire>("STATUS_PACK", "PC_MISALIGNED")});
    w.connect(builder.getOutputPin<Rewire, 32>("STATUS_PACK", "STATUS"),
        {builder.getInputPin<Mux2to1_32bit, 32>("STATUS_OUTPUT_MUX", "B")});
    w.connect(builder.getOutputPin<Mux2to1_32bit, 32>("PC_OUTPUT_MUX", "OUT"),
        {getOutputPin<32>("OUT_PC")});
    w.connect(builder.getOutputPin<Mux2to1_32bit, 32>("INSTRUCTION_OUTPUT_MUX", "OUT"),
        {getOutputPin<32>("OUT_INSTRUCTION")});
    w.connect(builder.getOutputPin<Mux2to1_32bit, 32>("STATUS_OUTPUT_MUX", "OUT"),
        {getOutputPin<32>("OUT_FETCH_STATUS")});
    w.finish();
}
