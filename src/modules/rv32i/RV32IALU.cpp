#include "modules/rv32i/RV32IALU.hpp"
#include "RV32IALUValue.hpp"
#include "modules/composite/WordALUValueModel.hpp"
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

// Compact forms of the six parts (2026-10-28). Each follows its own gates bit
// by bit with the gate rules (a LOW forces AND, a HIGH forces OR, X/Z
// otherwise give X; wiring passes Z unchanged), so unknown inputs give the
// same outputs as the gates; CompactFormEquivalenceTest compares them.
class ALUPartBehavioral : public BasicComponent {
protected:
    using Bits = std::vector<LogicValue>;
    ALUPartBehavioral(std::string name, void (*pins)(IOComponent*), const char* type)
        : BasicComponent(std::move(name), 1, pins), type_(type) {}
    Bits in(const char* name) { return getInputPinDynamic(name)->getValueAsVector(); }
    LogicValue bit(const char* name) { return getInputPinDynamic(name)->getValue(); }
    void out(Simulator& s, SimulationTime t, const char* name, const Bits& bits) { _updateOutputWireDynamic(s, name, bits, t); }
    void out(Simulator& s, SimulationTime t, const char* name, LogicValue value) { _updateOutputWire(s, name, value, t); }
public:
    const char* getTypeName() const override { return type_; }
private:
    const char* type_;
};
using circuit::logic_values::andGate;
using circuit::logic_values::orGate;
using circuit::logic_values::xorGate;
using circuit::logic_values::notGate;
using circuit::logic_values::mux2;
using Model = circuit::word::ALUValueModel<32, circuit::word::Carry::Prefix>;
Model::Word word(const std::vector<LogicValue>& bits) { Model::Word w; std::copy(bits.begin(), bits.end(), w.begin()); return w; }

class ControlBehavioral final : public ALUPartBehavioral {
public:
    explicit ControlBehavioral(std::string n) : ALUPartBehavioral(std::move(n), controlPins, RV32IALUControl::TypeName) {}
    void evaluate(SimulationTime t, Simulator& s) override {
        const auto op = in("OP");
        out(s, t, "LOCAL_OP", Bits{op[0], op[1], op[2]});
        out(s, t, "HIGH_HALF", op[3]); out(s, t, "OUT_OF_RANGE", op[4]);
        const auto not1 = notGate(op[1]);
        const auto valid = andGate(andGate(notGate(op[4]), notGate(op[2])), not1);
        out(s, t, "SUB", andGate(valid, orGate(op[3], op[0])));
        out(s, t, "SHIFT_LEFT", not1); out(s, t, "SHIFT_ARITHMETIC", op[0]);
    }
};
class AddSubBehavioral final : public ALUPartBehavioral {
public:
    explicit AddSubBehavioral(std::string n) : ALUPartBehavioral(std::move(n), addSubPins, RV32IALUAddSub::TypeName) {}
    void evaluate(SimulationTime t, Simulator& s) override {
        const auto sub = bit("SUB");
        auto b = in("B");
        for (auto& x : b) x = xorGate(x, sub);
        const auto r = Model::add(word(in("A")), word(b), sub);
        out(s, t, "OUT", Bits(r.out.begin(), r.out.end()));
        out(s, t, "C_OUT", r.carry);
        out(s, t, "OVERFLOW", xorGate(r.carry_into_sign, r.carry));
    }
};
class LogicBehavioral final : public ALUPartBehavioral {
public:
    explicit LogicBehavioral(std::string n) : ALUPartBehavioral(std::move(n), logicPins, RV32IALULogic::TypeName) {}
    void evaluate(SimulationTime t, Simulator& s) override {
        const auto a = in("A"), b = in("B");
        Bits all(32), any(32), differ(32);
        for (size_t i = 0; i < 32; ++i) { all[i] = andGate(a[i], b[i]); any[i] = orGate(a[i], b[i]); differ[i] = xorGate(a[i], b[i]); }
        out(s, t, "AND_OUT", all); out(s, t, "OR_OUT", any); out(s, t, "XOR_OUT", differ);
    }
};
class ShifterBehavioral final : public ALUPartBehavioral {
public:
    explicit ShifterBehavioral(std::string n) : ALUPartBehavioral(std::move(n), shifterPins, RV32IALUShifter::TypeName) {}
    void evaluate(SimulationTime t, Simulator& s) override {
        const auto a = in("A"), b = in("B");
        const auto left = bit("LEFT");
        const auto fill = andGate(andGate(a[31], bit("ARITHMETIC")), notGate(left));
        Bits current(32);
        for (size_t i = 0; i < 32; ++i) current[i] = mux2(a[i], a[31-i], left);
        for (size_t stage = 0; stage < 5; ++stage) {
            const size_t amount = size_t{1} << stage;
            Bits next(32);
            for (size_t i = 0; i < 32; ++i) next[i] = mux2(current[i], i + amount < 32 ? current[i + amount] : fill, b[stage]);
            current = std::move(next);
        }
        Bits result(32);
        for (size_t i = 0; i < 32; ++i) result[i] = mux2(current[i], current[31-i], left);
        out(s, t, "OUT", result);
    }
};
class CompareBehavioral final : public ALUPartBehavioral {
public:
    explicit CompareBehavioral(std::string n) : ALUPartBehavioral(std::move(n), comparePins, RV32IALUCompare::TypeName) {}
    void evaluate(SimulationTime t, Simulator& s) override {
        const auto diff = in("DIFF");
        const auto enable = bit("ENABLE");
        const auto lts = andGate(xorGate(diff[31], bit("OVERFLOW_IN")), enable);
        const auto ltu = andGate(notGate(bit("CARRY_IN")), enable);
        out(s, t, "EQ", andGate(Model::zero(word(diff)), enable));
        out(s, t, "LT_SIGNED", lts); out(s, t, "LT_UNSIGNED", ltu);
        Bits slt(32, LogicValue::LOW), sltu(32, LogicValue::LOW);
        slt[0] = lts; sltu[0] = ltu;
        out(s, t, "SLT_RESULT", slt); out(s, t, "SLTU_RESULT", sltu);
    }
};
class ResultSelectBehavioral final : public ALUPartBehavioral {
public:
    explicit ResultSelectBehavioral(std::string n) : ALUPartBehavioral(std::move(n), resultPins, RV32IALUResultSelect::TypeName) {}
    void evaluate(SimulationTime t, Simulator& s) override {
        const auto local = in("LOCAL_OP");
        const std::array<LogicValue, 4> op{local[0], local[1], local[2], bit("HIGH_HALF")};
        const auto valid = notGate(bit("OUT_OF_RANGE"));
        std::array<LogicValue, 12> terms;
        for (unsigned code = 0; code < terms.size(); ++code) {
            auto term = valid;
            for (unsigned i = 0; i < 4; ++i) term = andGate(term, code & (1u << i) ? op[i] : notGate(op[i]));
            terms[code] = term;
        }
        const std::array<LogicValue, 9> selects{orGate(terms[0], terms[1]), terms[2], terms[3], terms[4],
            orGate(orGate(terms[5], terms[6]), terms[7]), terms[8], terms[9], terms[10], terms[11]};
        const std::array<const char*, 9> names{"ARITHMETIC_RESULT", "AND_RESULT", "OR_RESULT", "XOR_RESULT",
            "SHIFT_RESULT", "SLT_RESULT", "SLTU_RESULT", "A", "B"};
        std::array<Bits, 9> data;
        for (size_t i = 0; i < names.size(); ++i) data[i] = in(names[i]);
        Bits result(32);
        for (size_t bitIndex = 0; bitIndex < 32; ++bitIndex) {
            auto value = andGate(data[0][bitIndex], selects[0]);
            for (size_t i = 1; i < names.size(); ++i) value = orGate(value, andGate(data[i][bitIndex], selects[i]));
            result[bitIndex] = value;
        }
        out(s, t, "OUT", result);
    }
};

template<typename T> circuit::FamilyFactory compact() {
    return [](const std::string& n, const std::shared_ptr<circuit::BuildContext>& c) {
        return Component::createWithContext<T>(c, n);
    };
}

template<typename T> circuit::FamilyFactory structural() {
    return [](const std::string& n, const std::shared_ptr<circuit::BuildContext>& c) {
        return Component::createWithContext<T>(c, n);
    };
}

// Each part is chosen by the build profile like any other family: its
// compact form follows its gates bit for bit (2026-10-28), so a structural
// ALU may hold compact parts.
std::shared_ptr<IOComponent> addStructural(ComponentBuilder& b, const circuit::ComponentFamily& f, const std::string& name) {
    return b.add(f, name);
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
    static const ComponentFamily f{"rv32i.alu.control", RV32IALUControl::TypeName, controlPins, structural<RV32IALUControl>(), compact<ControlBehavioral>()};
    return f;
}
const ComponentFamily& rv32iALUAddSub() {
    static const ComponentFamily f{"rv32i.alu.add-sub", RV32IALUAddSub::TypeName, addSubPins, structural<RV32IALUAddSub>(), compact<AddSubBehavioral>()};
    return f;
}
const ComponentFamily& rv32iALULogic() {
    static const ComponentFamily f{"rv32i.alu.logic", RV32IALULogic::TypeName, logicPins, structural<RV32IALULogic>(), compact<LogicBehavioral>()};
    return f;
}
const ComponentFamily& rv32iALUShifter() {
    static const ComponentFamily f{"rv32i.alu.shifter", RV32IALUShifter::TypeName, shifterPins, structural<RV32IALUShifter>(), compact<ShifterBehavioral>()};
    return f;
}
const ComponentFamily& rv32iALUCompare() {
    static const ComponentFamily f{"rv32i.alu.compare", RV32IALUCompare::TypeName, comparePins, structural<RV32IALUCompare>(), compact<CompareBehavioral>()};
    return f;
}
const ComponentFamily& rv32iALUResultSelect() {
    static const ComponentFamily f{"rv32i.alu.result-select", RV32IALUResultSelect::TypeName, resultPins, structural<RV32IALUResultSelect>(), compact<ResultSelectBehavioral>()};
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
    for (const auto& [family, test] : parts) {
        registration::registerStructural(catalog, *family, test, "stable-after-settle");
        registration::registerBehavioral(catalog, *family, test, "CompactFormEquivalenceTest", {}, "stable-after-settle");
    }
}
}
