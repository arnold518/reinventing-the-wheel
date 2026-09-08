#include "modules/composite/ALU32ControlDecoder.hpp"
#include "modules/composite/ALU32CompareUnit.hpp"
#include "modules/composite/ALU32ResultSelector.hpp"
#include "modules/composite/ALU32FlagUnit.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "modules/composite/ALU32.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/ALU32Direct.hpp"
#include "modules/composite/Logic32.hpp"
#include "modules/composite/Shifter32.hpp"
#include <memory>
#include <string>
#include <vector>

namespace {
void defineALU32Pins(IOComponent* self) {
    self->addPin<32>("A", PinType::INPUT);
    self->addPin<32>("B", PinType::INPUT);
    self->addPin<5>("OP", PinType::INPUT);
    self->addPin<32>("OUT", PinType::OUTPUT);
    self->addPin("ZERO", PinType::OUTPUT);
    self->addPin("EQ", PinType::OUTPUT);
    self->addPin("LT_SIGNED", PinType::OUTPUT);
    self->addPin("LT_UNSIGNED", PinType::OUTPUT);
    self->addPin("NEGATIVE", PinType::OUTPUT);
    self->addPin("CARRY_OUT", PinType::OUTPUT);
    self->addPin("OVERFLOW", PinType::OUTPUT);
}
} // namespace

namespace circuit::families {
const ComponentFamily ALU32{
    "rv32i.alu32",
    "ALU32",
    defineALU32Pins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::ALU32>(context, name);
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::ALU32Direct>(context, name);
    }};
}

ALU32::ALU32(std::string name)
    : IOComponent(std::move(name),
                  circuit::families::ALU32.pinInitializer()) {}

void ALU32::buildInternals(ComponentBuilder& builder) {
    circuit::detail::StructuralBuilder n(*this, builder);
    auto control = n.add<ALU32ControlDecoder>("CONTROL");
    // Keep these established paths and fidelity-selectable families intact.
    auto arith = builder.add(circuit::families::AddSub32, "ARITHMETIC");
    auto logic = builder.add(circuit::families::Logic32, "LOGIC");
    auto shift = builder.add(circuit::families::Shifter32, "SHIFT");
    auto compare = n.add<ALU32CompareUnit>("COMPARE");
    auto result = n.add<ALU32ResultSelector>("RESULT_SELECT");
    auto flags = n.add<ALU32FlagUnit>("FLAGS");
    n.connect(n.input("OP"), control->getInputPin<5>("OP"));
    for (const auto& unit : std::vector<std::shared_ptr<IOComponent>>{arith, logic, shift, result}) {
        n.connect(n.input("A"), unit->getInputPin<32>("A"));
        n.connect(n.input("B"), unit->getInputPin<32>("B"));
    }
    n.connect(control->getOutputPin("SUB"), arith->getInputPin("SUB"));
    n.connect(control->getOutputPin("SUB"), compare->getInputPin("ENABLE"));
    n.connect(control->getOutputPin("SHIFT_LEFT"), shift->getInputPin("LEFT"));
    n.connect(control->getOutputPin("SHIFT_ARITHMETIC"), shift->getInputPin("ARITHMETIC"));
    n.connect(control->getOutputPin<3>("LOCAL_OP"), result->getInputPin<3>("LOCAL_OP"));
    n.connect(control->getOutputPin("HIGH_HALF"), result->getInputPin("HIGH_HALF"));
    n.connect(control->getOutputPin("OUT_OF_RANGE"), result->getInputPin("OUT_OF_RANGE"));
    n.connect(arith->getOutputPin<32>("OUT"), compare->getInputPin<32>("DIFF"));
    n.connect(arith->getOutputPin("CARRY_OUT"), compare->getInputPin("CARRY_IN"));
    n.connect(arith->getOutputPin("OVERFLOW"), compare->getInputPin("OVERFLOW_IN"));
    n.connect(arith->getOutputPin<32>("OUT"), result->getInputPin<32>("ARITHMETIC_RESULT"));
    for (const auto* operation : {"AND", "OR", "XOR"})
        n.connect(logic->getOutputPin<32>(std::string(operation) + "_OUT"),
                  result->getInputPin<32>(std::string(operation) + "_RESULT"));
    n.connect(shift->getOutputPin<32>("OUT"), result->getInputPin<32>("SHIFT_RESULT"));
    n.connect(compare->getOutputPin<32>("SLT_RESULT"), result->getInputPin<32>("SLT_RESULT"));
    n.connect(compare->getOutputPin<32>("SLTU_RESULT"), result->getInputPin<32>("SLTU_RESULT"));
    for (const auto* name : {"EQ", "LT_SIGNED", "LT_UNSIGNED"})
        n.connect(compare->getOutputPin(name), n.output(name));
    n.connect(result->getOutputPin<32>("OUT"), n.output("OUT"));
    n.connect(result->getOutputPin<32>("OUT"), flags->getInputPin<32>("RESULT"));
    n.connect(control->getOutputPin("ARITHMETIC_ENABLE"), flags->getInputPin("ARITHMETIC_ENABLE"));
    n.connect(arith->getOutputPin("CARRY_OUT"), flags->getInputPin("ARITH_CARRY"));
    n.connect(arith->getOutputPin("OVERFLOW"), flags->getInputPin("ARITH_OVERFLOW"));
    for (const auto* name : {"ZERO", "NEGATIVE", "CARRY_OUT", "OVERFLOW"})
        n.connect(flags->getOutputPin(name), n.output(name));
    n.finish();
}
