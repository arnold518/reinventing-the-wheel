#include "components/ComponentBuilder.tpp"
#include "modules/composite/WordBuilder.hpp"
namespace circuit::word {
void buildIntegerALU(IOComponent& self,ComponentBuilder& builder,size_t width,ALUConfig config){
    WordBuilder n(self,builder,width,config);
    auto control = n.addStructural(controlFamily(config.operations), "CONTROL");
    // A structural integer datapath keeps its functional units structural.
    // Each width resolves its own configured family.
    auto arith = n.addWordStructural(Unit::AddSub, "ARITHMETIC");
    auto logic = n.addWordStructural(Unit::Logic, "LOGIC");
    auto shift = n.addWordStructural(Unit::Shift, "SHIFT");
    auto compare = n.addWordStructural(Unit::Compare, "COMPARE");
    auto result = n.addWordStructural(Unit::Result, "RESULT_SELECT");
    std::shared_ptr<IOComponent> flags;
    if (config.full_flags) flags = n.addWordStructural(Unit::Flags, "FLAGS");
    n.connect(n.input("OP"), control->getInputPin<5>("OP"));
    for (const auto& unit : std::vector<std::shared_ptr<IOComponent>>{arith, logic, shift, result}) {
        n.connect(n.input("A"), unit->getInputPinDynamic("A"));
        n.connect(n.input("B"), unit->getInputPinDynamic("B"));
    }
    n.connect(control->getOutputPin("SUB"), arith->getInputPin("SUB"));
    n.connect(control->getOutputPin("SUB"), compare->getInputPin("ENABLE"));
    n.connect(control->getOutputPin("SHIFT_LEFT"), shift->getInputPin("LEFT"));
    n.connect(control->getOutputPin("SHIFT_ARITHMETIC"), shift->getInputPin("ARITHMETIC"));
    n.connect(control->getOutputPin<3>("LOCAL_OP"), result->getInputPin<3>("LOCAL_OP"));
    n.connect(control->getOutputPin("HIGH_HALF"), result->getInputPin("HIGH_HALF"));
    n.connect(control->getOutputPin("OUT_OF_RANGE"), result->getInputPin("OUT_OF_RANGE"));
    n.connect(arith->getOutputPinDynamic("OUT"), compare->getInputPinDynamic("DIFF"));
    n.connect(arith->getOutputPin("CARRY_OUT"), compare->getInputPin("CARRY_IN"));
    n.connect(arith->getOutputPin("OVERFLOW"), compare->getInputPin("OVERFLOW_IN"));
    n.connect(arith->getOutputPinDynamic("OUT"), result->getInputPinDynamic("ARITHMETIC_RESULT"));
    for (const auto* operation : {"AND", "OR", "XOR"})
        n.connect(logic->getOutputPinDynamic(std::string(operation) + "_OUT"),
                  result->getInputPinDynamic(std::string(operation) + "_RESULT"));
    n.connect(shift->getOutputPinDynamic("OUT"), result->getInputPinDynamic("SHIFT_RESULT"));
    n.connect(compare->getOutputPinDynamic("SLT_RESULT"), result->getInputPinDynamic("SLT_RESULT"));
    n.connect(compare->getOutputPinDynamic("SLTU_RESULT"), result->getInputPinDynamic("SLTU_RESULT"));
    for (const auto* name : {"EQ", "LT_SIGNED", "LT_UNSIGNED"})
        n.connect(compare->getOutputPin(name), n.output(name));
    n.connect(result->getOutputPinDynamic("OUT"), n.output("OUT"));
    if (config.full_flags) {
        n.connect(result->getOutputPinDynamic("OUT"), flags->getInputPinDynamic("RESULT"));
        n.connect(control->getOutputPin("ARITHMETIC_ENABLE"), flags->getInputPin("ARITHMETIC_ENABLE"));
        n.connect(arith->getOutputPin("CARRY_OUT"), flags->getInputPin("ARITH_CARRY"));
        n.connect(arith->getOutputPin("OVERFLOW"), flags->getInputPin("ARITH_OVERFLOW"));
        for (const auto* name : {"ZERO", "NEGATIVE", "CARRY_OUT", "OVERFLOW"})
            n.connect(flags->getOutputPin(name), n.output(name));
    }
    n.finish();
}
}
