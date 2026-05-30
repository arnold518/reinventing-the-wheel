#include "modules/memory/MemoryBit.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "components/WireBuilder.hpp"
#include "modules/basic/DFlipFlop.hpp"
#include "modules/basic/Mux.hpp"

BEGIN_PINS(MemoryBit, IOComponent)
    INPUT_PIN("D")
    INPUT_PIN("WE")
    INPUT_PIN("CLK")
    INPUT_PIN("RST")
    OUTPUT_PIN("Q")
END_PINS()

void MemoryBit::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<Mux2to1>("WRITE_MUX");
    builder.addNewComponent<DFlipFlop>("STATE");

    builder.wire("D_to_write_mux")
        .fromInput("D")
        .to<Mux2to1>("WRITE_MUX", "B");

    builder.wire("WE_to_write_mux")
        .fromInput("WE")
        .to<Mux2to1>("WRITE_MUX", "SEL");

    builder.wire("CLK_to_state")
        .fromInput("CLK")
        .to<DFlipFlop>("STATE", "CLK");

    builder.wire("RST_to_state")
        .fromInput("RST")
        .to<DFlipFlop>("STATE", "RST");

    builder.wire("write_mux_to_state")
        .from<Mux2to1>("WRITE_MUX", "OUT")
        .to<DFlipFlop>("STATE", "D");

    builder.wire("state_feedback")
        .from<DFlipFlop>("STATE", "Q")
        .to<Mux2to1>("WRITE_MUX", "A")
        .toOutput("Q");
}
