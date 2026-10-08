#include "modules/composite/FullAdder.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/composite/HalfAdder.hpp"

BEGIN_PINS(FullAdder, IOComponent)
    INPUT_PIN("A")
    INPUT_PIN("B")
    INPUT_PIN("Carry_in")
    OUTPUT_PIN("Sum")
    OUTPUT_PIN("Carry_out")
END_PINS()

void FullAdder::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<HalfAdder>("HA1");
    builder.addNewComponent<HalfAdder>("HA2");
    builder.addNewComponent<ORGate>("OR1");

    w.connect(getInputPin("A"), builder.getInputPin<HalfAdder>("HA1", "A"));

    w.connect(getInputPin("B"), builder.getInputPin<HalfAdder>("HA1", "B"));

    w.connect(getInputPin("Carry_in"), builder.getInputPin<HalfAdder>("HA2", "B"));

    w.connect(builder.getOutputPin<HalfAdder>("HA1", "Sum"), builder.getInputPin<HalfAdder>("HA2", "A"));

    w.connect(builder.getOutputPin<HalfAdder>("HA1", "Carry"), builder.getInputPin<ORGate>("OR1", "A"));

    w.connect(builder.getOutputPin<HalfAdder>("HA2", "Carry"), builder.getInputPin<ORGate>("OR1", "B"));

    w.connect(builder.getOutputPin<HalfAdder>("HA2", "Sum"), getOutputPin("Sum"));

    w.connect(builder.getOutputPin<ORGate>("OR1", "OUT"), getOutputPin("Carry_out"));
    w.finish();
}
