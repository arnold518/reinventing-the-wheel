#include "modules/composite/ALU8.hpp"
#include "modules/composite/ArithmeticUnit8.hpp"
#include "modules/composite/LogicUnit8.hpp"
#include "modules/composite/BarrelShifter8.hpp"
#include "modules/composite/CompareSelect8.hpp"
#include "modules/composite/ALU8FlagUnit.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "modules/composite/ALU8ControlDecoder.hpp"
#include "modules/composite/ALU8ResultSelector.hpp"
#include "components/PinMacros.hpp"

ALU8::ALU8(std::string name)
    : IOComponent(std::move(name), circuit::families::ALU8.pinInitializer()) {}

void ALU8::buildInternals(ComponentBuilder& builder) {
    circuit::detail::Structural8Builder n(*this, builder);
    auto control = n.add(circuit::families::ALU8ControlDecoder, "CONTROL");
    auto arith = n.add(circuit::families::ArithmeticUnit8, "ARITHMETIC");
    auto logical = n.add(circuit::families::LogicUnit8, "LOGIC");
    auto shifter = n.add(circuit::families::BarrelShifter8, "SHIFTER");
    auto comparison = n.add(circuit::families::CompareSelect8, "COMPARE_SELECT");
    auto selector = n.add(circuit::families::ALU8ResultSelector, "RESULT_SELECT");
    auto flags = n.add(circuit::families::ALU8FlagUnit, "FLAGS");
    n.connect(n.input("OP"), control->getInputPin<5>("OP"));
    for (const auto& unit : std::vector<std::shared_ptr<IOComponent>>{arith, logical, shifter, comparison, selector}) {
        n.connect(n.input("A"), unit->getInputPin<8>("A"));
        n.connect(n.input("B"), unit->getInputPin<8>("B"));
    }
    n.connect(n.input("C_IN"), arith->getInputPin("C_IN"));
    n.connect(n.input("C_IN"), shifter->getInputPin("C_IN"));
    n.connect(control->getOutputPin<3>("ARITH_OP"), arith->getInputPin<3>("OP"));
    n.connect(control->getOutputPin<3>("LOCAL_OP"), logical->getInputPin<3>("OP"));
    n.connect(control->getOutputPin<3>("LOCAL_OP"), shifter->getInputPin<3>("OP"));
    n.connect(control->getOutputPin<3>("COMPARE_OP"), comparison->getInputPin<3>("OP"));
    n.connect(arith->getOutputPin<8>("OUT"), comparison->getInputPin<8>("DIFF"));
    n.connect(arith->getOutputPin("CARRY"), comparison->getInputPin("CARRY_IN"));
    n.connect(arith->getOutputPin("OVERFLOW"), comparison->getInputPin("OVERFLOW_IN"));
    n.connect(arith->getOutputPin<8>("OUT"), selector->getInputPin<8>("ARITHMETIC_RESULT"));
    n.connect(logical->getOutputPin<8>("OUT"), selector->getInputPin<8>("LOGIC_RESULT"));
    n.connect(shifter->getOutputPin<8>("OUT"), selector->getInputPin<8>("SHIFT_RESULT"));
    n.connect(comparison->getOutputPin<8>("OUT"), selector->getInputPin<8>("COMPARE_RESULT"));
    for (const auto* enable : {"ARITHMETIC_ENABLE", "LOGIC_ENABLE", "SHIFT_ENABLE",
                               "COMPARE_ENABLE", "PASS_A_ENABLE", "PASS_B_ENABLE"})
        n.connect(control->getOutputPin(enable), selector->getInputPin(enable));
    n.connect(selector->getOutputPin<8>("OUT"), n.output("OUT"));
    n.connect(selector->getOutputPin<8>("OUT"), flags->getInputPin<8>("RESULT"));
    n.connect(control->getOutputPin("ARITHMETIC_ENABLE"), flags->getInputPin("ARITHMETIC"));
    n.connect(control->getOutputPin("SHIFT_ENABLE"), flags->getInputPin("SHIFT"));
    n.connect(arith->getOutputPin("CARRY"), flags->getInputPin("ARITH_CARRY"));
    n.connect(arith->getOutputPin("OVERFLOW"), flags->getInputPin("ARITH_OVERFLOW"));
    n.connect(shifter->getOutputPin("CARRY"), flags->getInputPin("SHIFT_CARRY"));
    for (const auto* name : {"ZERO", "NEGATIVE", "CARRY", "OVERFLOW"})
        n.connect(flags->getOutputPin(name), n.output(name));
    n.connect(control->getOutputPin("INVALID_OP"), n.output("INVALID_OP"));
    n.finish();
}
