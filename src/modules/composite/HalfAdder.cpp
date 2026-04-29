#include "modules/composite/HalfAdder.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "components/WireBuilder.hpp"
#include "modules/basic/Gate.hpp"

BEGIN_PINS(HalfAdder, IOComponent)
    INPUT_PIN("A")
    INPUT_PIN("B")
    OUTPUT_PIN("Sum")
    OUTPUT_PIN("Carry")
END_PINS()

void HalfAdder::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<XORGate>("XOR1");
    builder.addNewComponent<ANDGate>("AND1");

    builder.wire("A_internal")
        .fromInput("A")
        .to<XORGate>("XOR1", "A")
        .to<ANDGate>("AND1", "A");

    builder.wire("B_internal")
        .fromInput("B")
        .to<XORGate>("XOR1", "B")
        .to<ANDGate>("AND1", "B");

    builder.wire("Sum_internal")
        .from<XORGate>("XOR1", "OUT")
        .toOutput("Sum");

    builder.wire("Carry_internal")
        .from<ANDGate>("AND1", "OUT")
        .toOutput("Carry");
}
