#include "modules/rv32i/RV32IPipelineControl.hpp"

#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "modules/utility/Rewire.hpp"
#include "RV32ILogicNetwork.hpp"
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
    builder.addNewComponent<BitSplitter<32>>("ADDRESS_SPLIT");
    builder.addNewComponent<BitSplitter<2>>("SIZE_SPLIT");
    builder.addNewComponent<ORGate>("LOW_ADDRESS_OR");
    builder.addNewComponent<ANDGate>("HALF_MISALIGNED");
    builder.addNewComponent<ANDGate>("WORD_MISALIGNED");
    builder.addNewComponent<ORGate>("MISALIGNED_OR");

    builder.addNewWire<32>(
        "ADDRESS_to_split",
        getInputPin<32>("ADDRESS"),
        {builder.getInputPin<BitSplitter<32>, 32>(
            "ADDRESS_SPLIT", "IN")});
    builder.addNewWire<2>(
        "SIZE_to_split",
        getInputPin<2>("SIZE"),
        {builder.getInputPin<BitSplitter<2>, 2>(
            "SIZE_SPLIT", "IN")});
    builder.addNewWire(
        "ADDRESS_bit0_fanout",
        builder.getOutputPin<BitSplitter<32>>(
            "ADDRESS_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("LOW_ADDRESS_OR", "A"),
         builder.getInputPin<ANDGate>(
             "HALF_MISALIGNED", "B")});
    builder.addNewWire(
        "ADDRESS_bit1_to_low_or",
        builder.getOutputPin<BitSplitter<32>>(
            "ADDRESS_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("LOW_ADDRESS_OR", "B")});
    builder.addNewWire(
        "SIZE_bit0_to_half",
        builder.getOutputPin<BitSplitter<2>>(
            "SIZE_SPLIT", "OUT_0"),
        {builder.getInputPin<ANDGate>(
            "HALF_MISALIGNED", "A")});
    builder.addNewWire(
        "SIZE_bit1_to_word",
        builder.getOutputPin<BitSplitter<2>>(
            "SIZE_SPLIT", "OUT_1"),
        {builder.getInputPin<ANDGate>(
            "WORD_MISALIGNED", "A")});
    builder.addNewWire(
        "LOW_ADDRESS_to_word",
        builder.getOutputPin<ORGate>(
            "LOW_ADDRESS_OR", "OUT"),
        {builder.getInputPin<ANDGate>(
            "WORD_MISALIGNED", "B")});
    builder.addNewWire(
        "HALF_to_misaligned_or",
        builder.getOutputPin<ANDGate>(
            "HALF_MISALIGNED", "OUT"),
        {builder.getInputPin<ORGate>("MISALIGNED_OR", "A")});
    builder.addNewWire(
        "WORD_to_misaligned_or",
        builder.getOutputPin<ANDGate>(
            "WORD_MISALIGNED", "OUT"),
        {builder.getInputPin<ORGate>("MISALIGNED_OR", "B")});
    builder.addNewWire(
        "MISALIGNED_to_output",
        builder.getOutputPin<ORGate>(
            "MISALIGNED_OR", "OUT"),
        {getOutputPin("MISALIGNED")});
}
