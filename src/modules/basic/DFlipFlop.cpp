#include "modules/basic/DFlipFlop.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "components/WireBuilder.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Latch.hpp"

BEGIN_PINS(DFlipFlop, IOComponent)
    INPUT_PIN("D")
    INPUT_PIN("CLK")
    INPUT_PIN("RST")
    OUTPUT_PIN("Q")
    OUTPUT_PIN("Q_BAR")
END_PINS()

void DFlipFlop::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<NOTGate>("NOT_CLK");
    builder.addNewComponent<GatedDLatch>("MASTER");
    builder.addNewComponent<GatedDLatch>("SLAVE");

    builder.wire("D_to_master")
        .fromInput("D")
        .to<GatedDLatch>("MASTER", "D");

    builder.wire("CLK_internal")
        .fromInput("CLK")
        .to<NOTGate>("NOT_CLK", "IN")
        .to<GatedDLatch>("SLAVE", "EN");

    builder.wire("not_clk_to_master_en")
        .from<NOTGate>("NOT_CLK", "OUT")
        .to<GatedDLatch>("MASTER", "EN");

    builder.wire("RST_to_latches")
        .fromInput("RST")
        .to<GatedDLatch>("MASTER", "RST")
        .to<GatedDLatch>("SLAVE", "RST");

    builder.wire("master_q_to_slave")
        .from<GatedDLatch>("MASTER", "Q")
        .to<GatedDLatch>("SLAVE", "D");

    builder.wire("slave_q_to_output")
        .from<GatedDLatch>("SLAVE", "Q")
        .toOutput("Q");

    builder.wire("slave_q_bar_to_output")
        .from<GatedDLatch>("SLAVE", "Q_BAR")
        .toOutput("Q_BAR");
}
