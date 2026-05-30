#include "modules/basic/Latch.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "components/WireBuilder.hpp"
#include "modules/basic/Gate.hpp"

BEGIN_PINS(SRLatch, IOComponent)
    INPUT_PIN("S_BAR")
    INPUT_PIN("R_BAR")
    OUTPUT_PIN("Q")
    OUTPUT_PIN("Q_BAR")
END_PINS()

void SRLatch::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<NANDGate>("NAND_Q");
    builder.addNewComponent<NANDGate>("NAND_Q_BAR");

    builder.wire("S_bar_to_q")
        .fromInput("S_BAR")
        .to<NANDGate>("NAND_Q", "A");

    builder.wire("R_bar_to_q_bar")
        .fromInput("R_BAR")
        .to<NANDGate>("NAND_Q_BAR", "A");

    builder.wire("q_feedback")
        .from<NANDGate>("NAND_Q", "OUT")
        .to<NANDGate>("NAND_Q_BAR", "B")
        .toOutput("Q");

    builder.wire("q_bar_feedback")
        .from<NANDGate>("NAND_Q_BAR", "OUT")
        .to<NANDGate>("NAND_Q", "B")
        .toOutput("Q_BAR");
}

BEGIN_PINS(GatedDLatch, IOComponent)
    INPUT_PIN("D")
    INPUT_PIN("EN")
    INPUT_PIN("RST")
    OUTPUT_PIN("Q")
    OUTPUT_PIN("Q_BAR")
END_PINS()

void GatedDLatch::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<NOTGate>("NOT_D");
    builder.addNewComponent<NOTGate>("NOT_RST");
    builder.addNewComponent<NANDGate>("S_DATA_BAR");
    builder.addNewComponent<NANDGate>("R_DATA_BAR");
    builder.addNewComponent<ORGate>("S_RESET_GATE");
    builder.addNewComponent<ANDGate>("R_RESET_GATE");
    builder.addNewComponent<SRLatch>("STATE");

    builder.wire("D_internal")
        .fromInput("D")
        .to<NOTGate>("NOT_D", "IN")
        .to<NANDGate>("S_DATA_BAR", "A");

    builder.wire("not_d_to_r_data")
        .from<NOTGate>("NOT_D", "OUT")
        .to<NANDGate>("R_DATA_BAR", "A");

    builder.wire("EN_to_data_gates")
        .fromInput("EN")
        .to<NANDGate>("S_DATA_BAR", "B")
        .to<NANDGate>("R_DATA_BAR", "B");

    builder.wire("RST_internal")
        .fromInput("RST")
        .to<NOTGate>("NOT_RST", "IN")
        .to<ORGate>("S_RESET_GATE", "B");

    builder.wire("not_rst_to_r_reset")
        .from<NOTGate>("NOT_RST", "OUT")
        .to<ANDGate>("R_RESET_GATE", "B");

    builder.wire("s_data_to_reset_gate")
        .from<NANDGate>("S_DATA_BAR", "OUT")
        .to<ORGate>("S_RESET_GATE", "A");

    builder.wire("r_data_to_reset_gate")
        .from<NANDGate>("R_DATA_BAR", "OUT")
        .to<ANDGate>("R_RESET_GATE", "A");

    builder.wire("s_reset_to_state")
        .from<ORGate>("S_RESET_GATE", "OUT")
        .to<SRLatch>("STATE", "S_BAR");

    builder.wire("r_reset_to_state")
        .from<ANDGate>("R_RESET_GATE", "OUT")
        .to<SRLatch>("STATE", "R_BAR");

    builder.wire("state_q_to_output")
        .from<SRLatch>("STATE", "Q")
        .toOutput("Q");

    builder.wire("state_q_bar_to_output")
        .from<SRLatch>("STATE", "Q_BAR")
        .toOutput("Q_BAR");
}
