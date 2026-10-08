#include "components/StructuralNetBuilder.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/utility/BitAdapter.hpp"
#include "RV32ILogicNetwork.hpp"

namespace {
using rv32i::detail::LogicNetwork;
using rv32i::detail::NetId;
} // namespace

RV32IMemoryAlignmentUnit::RV32IMemoryAlignmentUnit(
    std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IMemoryAlignmentUnit
              .pinInitializer()) {}

void RV32IMemoryAlignmentUnit::buildInternals(
    ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<32>>("ADDRESS_SPLIT");
    builder.addNewComponent<BitSplitter<2>>("SIZE_SPLIT");
    builder.addNewComponent<ORGate>("LOW_ADDRESS_OR");
    builder.addNewComponent<ANDGate>("HALF_MISALIGNED");
    builder.addNewComponent<ANDGate>("WORD_MISALIGNED");
    builder.addNewComponent<ORGate>("MISALIGNED_OR");

    w.connect(getInputPin<32>("ADDRESS"),
        {builder.getInputPin<BitSplitter<32>, 32>(
            "ADDRESS_SPLIT", "IN")});
    w.connect(getInputPin<2>("SIZE"),
        {builder.getInputPin<BitSplitter<2>, 2>(
            "SIZE_SPLIT", "IN")});
    w.connect(builder.getOutputPin<BitSplitter<32>>(
            "ADDRESS_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("LOW_ADDRESS_OR", "A"),
         builder.getInputPin<ANDGate>(
             "HALF_MISALIGNED", "B")});
    w.connect(builder.getOutputPin<BitSplitter<32>>(
            "ADDRESS_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("LOW_ADDRESS_OR", "B")});
    w.connect(builder.getOutputPin<BitSplitter<2>>(
            "SIZE_SPLIT", "OUT_0"),
        {builder.getInputPin<ANDGate>(
            "HALF_MISALIGNED", "A")});
    w.connect(builder.getOutputPin<BitSplitter<2>>(
            "SIZE_SPLIT", "OUT_1"),
        {builder.getInputPin<ANDGate>(
            "WORD_MISALIGNED", "A")});
    w.connect(builder.getOutputPin<ORGate>(
            "LOW_ADDRESS_OR", "OUT"),
        {builder.getInputPin<ANDGate>(
            "WORD_MISALIGNED", "B")});
    w.connect(builder.getOutputPin<ANDGate>(
            "HALF_MISALIGNED", "OUT"),
        {builder.getInputPin<ORGate>("MISALIGNED_OR", "A")});
    w.connect(builder.getOutputPin<ANDGate>(
            "WORD_MISALIGNED", "OUT"),
        {builder.getInputPin<ORGate>("MISALIGNED_OR", "B")});
    w.connect(builder.getOutputPin<ORGate>(
            "MISALIGNED_OR", "OUT"),
        {getOutputPin("MISALIGNED")});
    w.finish();
}
