#include "modules/basic/DFlipFlop.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "components/StructuralNetBuilder.hpp"
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
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<NOTGate>("NOT_CLK");
    builder.addNewComponent<GatedDLatch>("MASTER");
    builder.addNewComponent<GatedDLatch>("SLAVE");

    w.connect(getInputPin("D"), builder.getInputPin<GatedDLatch>("MASTER", "D"));

    w.connect(getInputPin("CLK"),
        {builder.getInputPin<NOTGate>("NOT_CLK", "IN"),
        builder.getInputPin<GatedDLatch>("SLAVE", "EN")});

    w.connect(builder.getOutputPin<NOTGate>("NOT_CLK", "OUT"), builder.getInputPin<GatedDLatch>("MASTER", "EN"));

    w.connect(getInputPin("RST"),
        {builder.getInputPin<GatedDLatch>("MASTER", "RST"),
        builder.getInputPin<GatedDLatch>("SLAVE", "RST")});

    w.connect(builder.getOutputPin<GatedDLatch>("MASTER", "Q"), builder.getInputPin<GatedDLatch>("SLAVE", "D"));

    w.connect(builder.getOutputPin<GatedDLatch>("SLAVE", "Q"), getOutputPin("Q"));

    w.connect(builder.getOutputPin<GatedDLatch>("SLAVE", "Q_BAR"), getOutputPin("Q_BAR"));
    w.finish();
}
