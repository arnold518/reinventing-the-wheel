#include "modules/composite/HalfAdder.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/Gate.hpp"

BEGIN_PINS(HalfAdder, IOComponent)
    INPUT_PIN("A")
    INPUT_PIN("B")
    OUTPUT_PIN("Sum")
    OUTPUT_PIN("Carry")
END_PINS()

void HalfAdder::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<XORGate>("XOR1");
    builder.addNewComponent<ANDGate>("AND1");

    w.connect(getInputPin("A"),
        {builder.getInputPin<XORGate>("XOR1", "A"),
        builder.getInputPin<ANDGate>("AND1", "A")});

    w.connect(getInputPin("B"),
        {builder.getInputPin<XORGate>("XOR1", "B"),
        builder.getInputPin<ANDGate>("AND1", "B")});

    w.connect(builder.getOutputPin<XORGate>("XOR1", "OUT"), getOutputPin("Sum"));

    w.connect(builder.getOutputPin<ANDGate>("AND1", "OUT"), getOutputPin("Carry"));
    w.finish();
}
