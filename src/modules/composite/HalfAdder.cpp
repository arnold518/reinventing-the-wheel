#include "modules/composite/HalfAdder.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/WireBuilder.hpp"
#include "components/PinMacros.hpp"
#include "modules/basic/Gate.hpp" // For XORGate and ANDGate

// Using new PinMacros API (Proposal 2)
BEGIN_PINS(HalfAdder, IOComponent)
    INPUT_PIN("A")
    INPUT_PIN("B")
    OUTPUT_PIN("Sum")
    OUTPUT_PIN("Carry")
END_PINS()

void HalfAdder::buildInternals(ComponentBuilder& builder) {
    // --- 1. Create the internal components ---
    builder.addNewComponent<XORGate>("XOR1");
    builder.addNewComponent<ANDGate>("AND1");

    // --- 2. Wire using new WireBuilder API (Proposal 3) ---

    // Fan-out: my input A → both gates
    builder.wire("A_internal")
        .fromInput("A")
        .to<XORGate>("XOR1", "A")
        .to<ANDGate>("AND1", "A");

    // Fan-out: my input B → both gates
    builder.wire("B_internal")
        .fromInput("B")
        .to<XORGate>("XOR1", "B")
        .to<ANDGate>("AND1", "B");

    // Internal XOR output → my Sum output
    builder.wire("Sum_internal")
        .from<XORGate>("XOR1", "OUT")
        .toOutput("Sum");

    // Internal AND output → my Carry output
    builder.wire("Carry_internal")
        .from<ANDGate>("AND1", "OUT")
        .toOutput("Carry");
}
