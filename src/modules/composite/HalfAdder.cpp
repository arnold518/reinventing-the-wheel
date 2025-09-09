#include "modules/composite/HalfAdder.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp" // For XORGate and ANDGate

HalfAdder::HalfAdder(std::string name)
    : IOComponent(std::move(name), 
      // This lambda defines the EXTERNAL interface of the HalfAdder
      [](IOComponent* self) {
          self->addPin("A", PinType::INPUT);
          self->addPin("B", PinType::INPUT);
          self->addPin("Sum", PinType::OUTPUT);
          self->addPin("Carry", PinType::OUTPUT);
      })
{}

void HalfAdder::buildInternals(ComponentBuilder& builder) {
    // --- 1. Create the internal components ---
    // The builder's context is now inside this HalfAdder, so these
    // will be created as children.
    builder.addNewComponent<XORGate>("XOR1");
    builder.addNewComponent<ANDGate>("AND1");

    // --- 2. Wire the external inputs to the internal gates ---
    // builder.getInputPin("A") gets this HalfAdder's OWN "A" pin.
    // The wire connects this parent pin to the children's input pins.
    builder.addNewWire("A_internal",
        builder.getInputPin("A"), {
            builder.getInputPin<XORGate>("XOR1", "A"),
            builder.getInputPin<ANDGate>("AND1", "A")
        }
    );
    builder.addNewWire("B_internal",
        builder.getInputPin("B"), {
            builder.getInputPin<XORGate>("XOR1", "B"),
            builder.getInputPin<ANDGate>("AND1", "B")
        }
    );

    // --- 3. Wire the internal gates to the external outputs ---
    // The output of XOR1 becomes the source for the wire connected
    // to this HalfAdder's OWN "Sum" output pin.
    builder.addNewWire("Sum_internal",
        builder.getOutputPin<XORGate>("XOR1", "OUT"),
        { builder.getOutputPin("Sum") }
    );
    builder.addNewWire("Carry_internal",
        builder.getOutputPin<ANDGate>("AND1", "OUT"),
        { builder.getOutputPin("Carry") }
    );
}