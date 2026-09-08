#include "modules/composite/ALU32ControlDecoder.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"

BEGIN_PINS(ALU32ControlDecoder, IOComponent)
    INPUT_PIN_WIDTH("OP", 5)
    OUTPUT_PIN_WIDTH("LOCAL_OP", 3)
    OUTPUT_PIN("HIGH_HALF")
    OUTPUT_PIN("OUT_OF_RANGE")
    OUTPUT_PIN("ARITHMETIC_ENABLE")
    OUTPUT_PIN("SUB")
    OUTPUT_PIN("SHIFT_LEFT")
    OUTPUT_PIN("SHIFT_ARITHMETIC")
END_PINS()

void ALU32ControlDecoder::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralBuilder n(*this, builder);
    auto op = n.split<5>("OP_SPLIT", n.input("OP"));
    std::array<circuit::detail::StructuralBuilder::Signal, 3> low{op[0], op[1], op[2]};
    n.connect(n.join<3>("OP_LOW_JOIN", low), n.output("LOCAL_OP"));
    n.connect(op[3], n.output("HIGH_HALF"));
    n.connect(op[4], n.output("OUT_OF_RANGE"));
    auto not4 = n.invert("NOT_OP4", op[4]);
    auto not2 = n.invert("NOT_OP2", op[2]);
    auto not1 = n.invert("NOT_OP1", op[1]);
    auto selector = n.binary<ORGate>("SUB_SELECTOR_OR", op[3], op[0]);
    auto valid1 = n.binary<ANDGate>("ARITHMETIC_VALID_1", not4, not2);
    auto valid = n.binary<ANDGate>("ARITHMETIC_VALID_2", valid1, not1);
    n.connect(valid, n.output("ARITHMETIC_ENABLE"));
    n.connect(n.binary<ANDGate>("SUB_CONTROL", valid, selector), n.output("SUB"));
    n.connect(not1, n.output("SHIFT_LEFT"));
    n.connect(op[0], n.output("SHIFT_ARITHMETIC"));
    n.finish();
}
