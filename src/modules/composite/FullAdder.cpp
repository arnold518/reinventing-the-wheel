#include "modules/composite/FullAdder.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/PinMacros.hpp"
#include "components/WireBuilder.hpp"
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
    builder.addNewComponent<HalfAdder>("HA1");
    builder.addNewComponent<HalfAdder>("HA2");
    builder.addNewComponent<ORGate>("OR1");

    builder.wire("A_internal")
        .fromInput("A")
        .to<HalfAdder>("HA1", "A");

    builder.wire("B_internal")
        .fromInput("B")
        .to<HalfAdder>("HA1", "B");

    builder.wire("Cin_internal")
        .fromInput("Carry_in")
        .to<HalfAdder>("HA2", "B");

    builder.wire("HA1_Sum_to_HA2_A")
        .from<HalfAdder>("HA1", "Sum")
        .to<HalfAdder>("HA2", "A");

    builder.wire("HA1_Carry_to_OR")
        .from<HalfAdder>("HA1", "Carry")
        .to<ORGate>("OR1", "A");

    builder.wire("HA2_Carry_to_OR")
        .from<HalfAdder>("HA2", "Carry")
        .to<ORGate>("OR1", "B");

    builder.wire("Sum_internal")
        .from<HalfAdder>("HA2", "Sum")
        .toOutput("Sum");

    builder.wire("Carry_out_internal")
        .from<ORGate>("OR1", "OUT")
        .toOutput("Carry_out");
}
