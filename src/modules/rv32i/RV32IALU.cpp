#include "modules/rv32i/RV32IALU.hpp"
#include "RV32IALUValue.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/Component.tpp"
#include "components/selection/FamilyRegistration.hpp"
#include "modules/composite/WordSliceBuilder.hpp"
#include "basic/LogicValueHelpers.hpp"
#include <array>

namespace {
using Net = circuit::detail::WordSliceBuilder;

void aluPins(IOComponent* p) {
    p->addPin<32>("A", PinType::INPUT); p->addPin<32>("B", PinType::INPUT);
    p->addPin<5>("OP", PinType::INPUT);
    p->addPin<32>("OUT", PinType::OUTPUT);
    for (auto n : {"EQ", "LT_SIGNED", "LT_UNSIGNED"}) p->addPin(n, PinType::OUTPUT);
}
void controlPins(IOComponent* p) {
    p->addPin<5>("OP", PinType::INPUT); p->addPin<3>("LOCAL_OP", PinType::OUTPUT);
    for (auto n : {"HIGH_HALF", "OUT_OF_RANGE", "SUB", "SHIFT_LEFT", "SHIFT_ARITHMETIC"}) p->addPin(n, PinType::OUTPUT);
}
void addSubPins(IOComponent* p) {
    p->addPin<32>("A", PinType::INPUT); p->addPin<32>("B", PinType::INPUT); p->addPin("SUB", PinType::INPUT);
    p->addPin<32>("OUT", PinType::OUTPUT); p->addPin("C_OUT", PinType::OUTPUT); p->addPin("OVERFLOW", PinType::OUTPUT);
}
void logicPins(IOComponent* p) {
    p->addPin<32>("A", PinType::INPUT); p->addPin<32>("B", PinType::INPUT);
    for (auto n : {"AND_OUT", "OR_OUT", "XOR_OUT"}) p->addPin<32>(n, PinType::OUTPUT);
}
void shifterPins(IOComponent* p) {
    p->addPin<32>("A", PinType::INPUT); p->addPin<32>("B", PinType::INPUT);
    p->addPin("LEFT", PinType::INPUT); p->addPin("ARITHMETIC", PinType::INPUT);
    p->addPin<32>("OUT", PinType::OUTPUT);
}
void comparePins(IOComponent* p) {
    p->addPin<32>("DIFF", PinType::INPUT);
    for (auto n : {"CARRY_IN", "OVERFLOW_IN", "ENABLE"}) p->addPin(n, PinType::INPUT);
    for (auto n : {"EQ", "LT_SIGNED", "LT_UNSIGNED"}) p->addPin(n, PinType::OUTPUT);
    p->addPin<32>("SLT_RESULT", PinType::OUTPUT); p->addPin<32>("SLTU_RESULT", PinType::OUTPUT);
}
void resultPins(IOComponent* p) {
    for (auto n : {"ARITHMETIC_RESULT", "AND_RESULT", "OR_RESULT", "XOR_RESULT", "SHIFT_RESULT",
                   "SLT_RESULT", "SLTU_RESULT", "A", "B"}) p->addPin<32>(n, PinType::INPUT);
    p->addPin<3>("LOCAL_OP", PinType::INPUT);
    p->addPin("HIGH_HALF", PinType::INPUT); p->addPin("OUT_OF_RANGE", PinType::INPUT);
    p->addPin<32>("OUT", PinType::OUTPUT);
}

// Compact RV32IALU: the shared RISC-V value rule. Its equivalence domain is
// known binary inputs; any X/Z bit on A, B or OP makes every output unknown.
class RV32IALUBehavioral final : public BasicComponent {
public:
    explicit RV32IALUBehavioral(std::string name) : BasicComponent(std::move(name), 1, aluPins) {}
    const char* getTypeName() const override { return RV32IALU::TypeName; }
    void evaluate(SimulationTime t, Simulator& s) override {
        bool known = true;
        for (const char* name : {"A", "B", "OP"})
            known &= circuit::logic_values::allKnown(getInputPinDynamic(name)->getValueAsVector());
        if (!known) {
            for (const auto& [name, pin] : getAllOutputPins())
                _updateOutputWireDynamic(s, name, std::vector<LogicValue>(pin->getWidth(), LogicValue::UNKNOWN), t);
            return;
        }
        namespace value = ::rv32i::alu_value;
        using circuit::logic_values::logic;
        const uint32_t a = getInputPin<32>("A")->getValueAsUInt64(), b = getInputPin<32>("B")->getValueAsUInt64();
        const uint8_t op = getInputPin<5>("OP")->getValueAsUInt64();
        _updateOutputWire<32>(s, "OUT", value::result<32>(op, a, b), t);
        namespace ALUOp = ::rv32i::ALUOp;
        const bool compare = op == ALUOp::SUB || op == ALUOp::SLT || op == ALUOp::SLTU;
        _updateOutputWire(s, "EQ", logic(compare && a == b), t);
        _updateOutputWire(s, "LT_SIGNED", logic(compare && value::signedValue<32>(a) < value::signedValue<32>(b)), t);
        _updateOutputWire(s, "LT_UNSIGNED", logic(compare && a < b), t);
    }
};

template<typename T> circuit::FamilyFactory structural() {
    return [](const std::string& n, const std::shared_ptr<circuit::BuildContext>& c) {
        return Component::createWithContext<T>(c, n);
    };
}

// Inside the structural ALU every part stays gate level.
std::shared_ptr<IOComponent> addStructural(ComponentBuilder& b, const circuit::ComponentFamily& f, const std::string& name) {
    if (!b.getBuildContext()) return b.add(f, name);
    auto request = f.request(name);
    request.required_fidelity = circuit::Fidelity::Structural;
    return b.addByContract(std::move(request));
}
}

namespace circuit::families {
const ComponentFamily& rv32iALU() {
    static const ComponentFamily f{"rv32i.alu", RV32IALU::TypeName, aluPins, structural<RV32IALU>(),
        [](const std::string& n, const std::shared_ptr<BuildContext>& c) {
            return Component::createWithContext<RV32IALUBehavioral>(c, n);
        }};
    return f;
}
const ComponentFamily& rv32iALUControl() {
    static const ComponentFamily f{"rv32i.alu.control", RV32IALUControl::TypeName, controlPins, structural<RV32IALUControl>()};
    return f;
}
const ComponentFamily& rv32iALUAddSub() {
    static const ComponentFamily f{"rv32i.alu.add-sub", RV32IALUAddSub::TypeName, addSubPins, structural<RV32IALUAddSub>()};
    return f;
}
const ComponentFamily& rv32iALULogic() {
    static const ComponentFamily f{"rv32i.alu.logic", RV32IALULogic::TypeName, logicPins, structural<RV32IALULogic>()};
    return f;
}
const ComponentFamily& rv32iALUShifter() {
    static const ComponentFamily f{"rv32i.alu.shifter", RV32IALUShifter::TypeName, shifterPins, structural<RV32IALUShifter>()};
    return f;
}
const ComponentFamily& rv32iALUCompare() {
    static const ComponentFamily f{"rv32i.alu.compare", RV32IALUCompare::TypeName, comparePins, structural<RV32IALUCompare>()};
    return f;
}
const ComponentFamily& rv32iALUResultSelect() {
    static const ComponentFamily f{"rv32i.alu.result-select", RV32IALUResultSelect::TypeName, resultPins, structural<RV32IALUResultSelect>()};
    return f;
}
}

RV32IALU::RV32IALU(std::string n) : IOComponent(std::move(n), aluPins) {}
RV32IALUControl::RV32IALUControl(std::string n) : IOComponent(std::move(n), controlPins) {}
RV32IALUAddSub::RV32IALUAddSub(std::string n) : IOComponent(std::move(n), addSubPins) {}
RV32IALULogic::RV32IALULogic(std::string n) : IOComponent(std::move(n), logicPins) {}
RV32IALUShifter::RV32IALUShifter(std::string n) : IOComponent(std::move(n), shifterPins) {}
RV32IALUCompare::RV32IALUCompare(std::string n) : IOComponent(std::move(n), comparePins) {}
RV32IALUResultSelect::RV32IALUResultSelect(std::string n) : IOComponent(std::move(n), resultPins) {}

// OP[3] picks the subtractor and the shift kind; OP[4] marks codes 16-31.
void RV32IALUControl::buildInternals(ComponentBuilder& builder) {
    Net n(*this, builder);
    auto op = n.split<5>("OP_SPLIT", n.input("OP"));
    std::array<Net::Signal, 3> low{op[0], op[1], op[2]};
    n.connect(n.join<3>("OP_LOW_JOIN", low), n.output("LOCAL_OP"));
    n.connect(op[3], n.output("HIGH_HALF"));
    n.connect(op[4], n.output("OUT_OF_RANGE"));
    auto not4 = n.invert("NOT_OP4", op[4]);
    auto not2 = n.invert("NOT_OP2", op[2]);
    auto not1 = n.invert("NOT_OP1", op[1]);
    auto selector = n.binary<ORGate>("SUB_SELECTOR_OR", op[3], op[0]);
    auto valid1 = n.binary<ANDGate>("ARITHMETIC_VALID_1", not4, not2);
    auto valid = n.binary<ANDGate>("ARITHMETIC_VALID_2", valid1, not1);
    n.connect(n.binary<ANDGate>("SUB_CONTROL", valid, selector), n.output("SUB"));
    n.connect(not1, n.output("SHIFT_LEFT"));
    n.connect(op[0], n.output("SHIFT_ARITHMETIC"));
    n.finish();
}

void RV32IALU::buildInternals(ComponentBuilder& builder) {
    Net n(*this, builder);
    auto control = addStructural(builder, circuit::families::rv32iALUControl(), "CONTROL");
    auto arith = addStructural(builder, circuit::families::rv32iALUAddSub(), "ARITHMETIC");
    auto logic = addStructural(builder, circuit::families::rv32iALULogic(), "LOGIC");
    auto shift = addStructural(builder, circuit::families::rv32iALUShifter(), "SHIFT");
    auto compare = addStructural(builder, circuit::families::rv32iALUCompare(), "COMPARE");
    auto result = addStructural(builder, circuit::families::rv32iALUResultSelect(), "RESULT_SELECT");
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
    n.connect(arith->getOutputPin("C_OUT"), compare->getInputPin("CARRY_IN"));
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
    n.finish();
}

namespace rv32i {
void registerALU(circuit::ComponentCatalog& catalog) {
    using namespace circuit;
    const auto& alu = families::rv32iALU();
    registration::registerStructural(catalog, alu, "RV32IALUTest", "stable-after-settle", {}, {}, "known-binary");
    registration::registerBehavioral(catalog, alu, "RV32IALUTest", "RV32IALUTest", {}, "stable-after-settle", "known-binary");
    const std::array<std::pair<const ComponentFamily*, const char*>, 6> parts{{
        {&families::rv32iALUControl(), "RV32IALUControlTest"}, {&families::rv32iALUAddSub(), "RV32IALUAddSubTest"},
        {&families::rv32iALULogic(), "RV32IALULogicTest"}, {&families::rv32iALUShifter(), "RV32IALUShifterTest"},
        {&families::rv32iALUCompare(), "RV32IALUCompareTest"}, {&families::rv32iALUResultSelect(), "RV32IALUResultSelectTest"}}};
    for (const auto& [family, test] : parts) registration::registerStructural(catalog, *family, test, "stable-after-settle");
}
}
