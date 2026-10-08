#include "modules/basic/Latch.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/Gate.hpp"

BEGIN_PINS(SRLatch, IOComponent)
    INPUT_PIN("S_BAR")
    INPUT_PIN("R_BAR")
    OUTPUT_PIN("Q")
    OUTPUT_PIN("Q_BAR")
END_PINS()

void SRLatch::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<NANDGate>("NAND_Q");
    builder.addNewComponent<NANDGate>("NAND_Q_BAR");

    w.connect(getInputPin("S_BAR"), builder.getInputPin<NANDGate>("NAND_Q", "A"));

    w.connect(getInputPin("R_BAR"), builder.getInputPin<NANDGate>("NAND_Q_BAR", "A"));

    w.connect(builder.getOutputPin<NANDGate>("NAND_Q", "OUT"),
        {builder.getInputPin<NANDGate>("NAND_Q_BAR", "B"),
        getOutputPin("Q")});

    w.connect(builder.getOutputPin<NANDGate>("NAND_Q_BAR", "OUT"),
        {builder.getInputPin<NANDGate>("NAND_Q", "B"),
        getOutputPin("Q_BAR")});
    w.finish();
}

BEGIN_PINS(GatedDLatch, IOComponent)
    INPUT_PIN("D")
    INPUT_PIN("EN")
    INPUT_PIN("RST")
    OUTPUT_PIN("Q")
    OUTPUT_PIN("Q_BAR")
END_PINS()

void GatedDLatch::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<NOTGate>("NOT_D");
    builder.addNewComponent<NOTGate>("NOT_RST");
    builder.addNewComponent<NANDGate>("S_DATA_BAR");
    builder.addNewComponent<NANDGate>("R_DATA_BAR");
    builder.addNewComponent<ORGate>("S_RESET_GATE");
    builder.addNewComponent<ANDGate>("R_RESET_GATE");
    builder.addNewComponent<SRLatch>("STATE");

    w.connect(getInputPin("D"),
        {builder.getInputPin<NOTGate>("NOT_D", "IN"),
        builder.getInputPin<NANDGate>("S_DATA_BAR", "A")});

    w.connect(builder.getOutputPin<NOTGate>("NOT_D", "OUT"), builder.getInputPin<NANDGate>("R_DATA_BAR", "A"));

    w.connect(getInputPin("EN"),
        {builder.getInputPin<NANDGate>("S_DATA_BAR", "B"),
        builder.getInputPin<NANDGate>("R_DATA_BAR", "B")});

    w.connect(getInputPin("RST"),
        {builder.getInputPin<NOTGate>("NOT_RST", "IN"),
        builder.getInputPin<ORGate>("S_RESET_GATE", "B")});

    w.connect(builder.getOutputPin<NOTGate>("NOT_RST", "OUT"), builder.getInputPin<ANDGate>("R_RESET_GATE", "B"));

    w.connect(builder.getOutputPin<NANDGate>("S_DATA_BAR", "OUT"), builder.getInputPin<ORGate>("S_RESET_GATE", "A"));

    w.connect(builder.getOutputPin<NANDGate>("R_DATA_BAR", "OUT"), builder.getInputPin<ANDGate>("R_RESET_GATE", "A"));

    w.connect(builder.getOutputPin<ORGate>("S_RESET_GATE", "OUT"), builder.getInputPin<SRLatch>("STATE", "S_BAR"));

    w.connect(builder.getOutputPin<ANDGate>("R_RESET_GATE", "OUT"), builder.getInputPin<SRLatch>("STATE", "R_BAR"));

    w.connect(builder.getOutputPin<SRLatch>("STATE", "Q"), getOutputPin("Q"));

    w.connect(builder.getOutputPin<SRLatch>("STATE", "Q_BAR"), getOutputPin("Q_BAR"));
    w.finish();
}
