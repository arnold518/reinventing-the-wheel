#include "modules/composite/FullAdder.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/composite/HalfAdder.hpp"
#include "modules/basic/Gate.hpp"

FullAdder::FullAdder(std::string name)
    : IOComponent(std::move(name), 
      // This lambda defines the EXTERNAL interface of the FullAdder
      [](IOComponent* self) {
          self->addPin("A", PinType::INPUT);
          self->addPin("B", PinType::INPUT);
          self->addPin("Carry_in", PinType::INPUT);
          self->addPin("Sum", PinType::OUTPUT);
          self->addPin("Carry_out", PinType::OUTPUT);
      })
{}

void FullAdder::buildInternals(ComponentBuilder& builder) {
    // 1. Create internal components
    builder.addNewComponent<HalfAdder>("HA1");
    builder.addNewComponent<HalfAdder>("HA2");
    builder.addNewComponent<ORGate>("OR1");

    // 2. Wire the primary inputs
    builder.addNewWire("A_internal", builder.getInputPin("A"), { builder.getInputPin<HalfAdder>("HA1", "A") });
    builder.addNewWire("B_internal", builder.getInputPin("B"), { builder.getInputPin<HalfAdder>("HA1", "B") });
    
    builder.addNewWire("Cin_internal", builder.getInputPin("Carry_in"), { builder.getInputPin<HalfAdder>("HA2", "B") });

    // 3. Wire the first HalfAdder's output to the second's input
    builder.addNewWire("HA1_Sum_to_HA2_A",
        builder.getOutputPin<HalfAdder>("HA1", "Sum"), 
        { builder.getInputPin<HalfAdder>("HA2", "A") }
    );

    // 4. Wire the carry outputs to the OR gate
    builder.addNewWire("HA1_Carry_to_OR",
        builder.getOutputPin<HalfAdder>("HA1", "Carry"), 
        { builder.getInputPin<ORGate>("OR1", "A") }
    );
    builder.addNewWire("HA2_Carry_to_OR",
        builder.getOutputPin<HalfAdder>("HA2", "Carry"), 
        { builder.getInputPin<ORGate>("OR1", "B") }
    );

    // 5. Wire the final results to the FullAdder's output pins
    builder.addNewWire("Sum_internal",
        builder.getOutputPin<HalfAdder>("HA2", "Sum"), 
        { builder.getOutputPin("Sum") }
    );
    builder.addNewWire("Carry_out_internal",
        builder.getOutputPin<ORGate>("OR1", "OUT"), 
        { builder.getOutputPin("Carry_out") }
    );
}