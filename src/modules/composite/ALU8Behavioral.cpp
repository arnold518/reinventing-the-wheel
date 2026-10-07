#include "modules/composite/Adder8.hpp"
#include "modules/composite/AddSub8.hpp"
#include "modules/composite/ArithmeticUnit8.hpp"
#include "modules/composite/LogicUnit8.hpp"
#include "modules/composite/BarrelShifter8.hpp"
#include "modules/composite/ZeroDetect8.hpp"
#include "modules/composite/CompareSelect8.hpp"
#include "modules/composite/ALU8FlagUnit.hpp"
#include "modules/composite/ALU8ControlDecoder.hpp"
#include "modules/composite/ALU8ResultSelector.hpp"
#include "modules/composite/ALU8.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.tpp"
#include "modules/composite/ALU8ValueModel.hpp"
#include <algorithm>
#include <type_traits>

namespace {
namespace value = circuit::detail::alu8;

void defineAdder8Pins(IOComponent* self) {
    self->addPin<8>("A", PinType::INPUT);
    self->addPin<8>("B", PinType::INPUT);
    self->addPin("Cin", PinType::INPUT);
    self->addPin<8>("Sum", PinType::OUTPUT);
    self->addPin("Cout", PinType::OUTPUT);
}

void defineAddSub8Pins(IOComponent* self) {
    self->addPin<8>("A", PinType::INPUT);
    self->addPin<8>("B", PinType::INPUT);
    self->addPin("SUB", PinType::INPUT);
    self->addPin("C_IN", PinType::INPUT);
    self->addPin<8>("OUT", PinType::OUTPUT);
    self->addPin("CARRY_OUT", PinType::OUTPUT);
    self->addPin("OVERFLOW", PinType::OUTPUT);
}

void defineArithmeticUnit8Pins(IOComponent* self) {
    self->addPin<8>("A", PinType::INPUT);
    self->addPin<8>("B", PinType::INPUT);
    self->addPin<3>("OP", PinType::INPUT);
    self->addPin("C_IN", PinType::INPUT);
    self->addPin<8>("OUT", PinType::OUTPUT);
    self->addPin("CARRY", PinType::OUTPUT);
    self->addPin("OVERFLOW", PinType::OUTPUT);
    self->addPin("INVALID_OP", PinType::OUTPUT);
}

void defineLogicUnit8Pins(IOComponent* self) {
    self->addPin<8>("A", PinType::INPUT);
    self->addPin<8>("B", PinType::INPUT);
    self->addPin<3>("OP", PinType::INPUT);
    self->addPin<8>("OUT", PinType::OUTPUT);
}

void defineBarrelShifter8Pins(IOComponent* self) {
    self->addPin<8>("A", PinType::INPUT);
    self->addPin<8>("B", PinType::INPUT);
    self->addPin<3>("OP", PinType::INPUT);
    self->addPin("C_IN", PinType::INPUT);
    self->addPin<8>("OUT", PinType::OUTPUT);
    self->addPin("CARRY", PinType::OUTPUT);
}

void defineZeroDetect8Pins(IOComponent* self) {
    self->addPin<8>("A", PinType::INPUT);
    self->addPin("ZERO", PinType::OUTPUT);
}

void defineCompareSelect8Pins(IOComponent* self) {
    self->addPin<8>("A", PinType::INPUT);
    self->addPin<8>("B", PinType::INPUT);
    self->addPin<8>("DIFF", PinType::INPUT);
    self->addPin("CARRY_IN", PinType::INPUT);
    self->addPin("OVERFLOW_IN", PinType::INPUT);
    self->addPin<3>("OP", PinType::INPUT);
    self->addPin<8>("OUT", PinType::OUTPUT);
}

void defineALU8FlagUnitPins(IOComponent* self) {
    self->addPin<8>("RESULT", PinType::INPUT);
    self->addPin("ARITHMETIC", PinType::INPUT);
    self->addPin("SHIFT", PinType::INPUT);
    self->addPin("ARITH_CARRY", PinType::INPUT);
    self->addPin("ARITH_OVERFLOW", PinType::INPUT);
    self->addPin("SHIFT_CARRY", PinType::INPUT);
    self->addPin("ZERO", PinType::OUTPUT);
    self->addPin("NEGATIVE", PinType::OUTPUT);
    self->addPin("CARRY", PinType::OUTPUT);
    self->addPin("OVERFLOW", PinType::OUTPUT);
}

void defineALU8ControlDecoderPins(IOComponent* self) {
    self->addPin<5>("OP", PinType::INPUT);
    self->addPin<3>("LOCAL_OP", PinType::OUTPUT);
    self->addPin<3>("ARITH_OP", PinType::OUTPUT);
    self->addPin<3>("COMPARE_OP", PinType::OUTPUT);
    self->addPin("ARITHMETIC_ENABLE", PinType::OUTPUT);
    self->addPin("LOGIC_ENABLE", PinType::OUTPUT);
    self->addPin("SHIFT_ENABLE", PinType::OUTPUT);
    self->addPin("COMPARE_ENABLE", PinType::OUTPUT);
    self->addPin("PASS_A_ENABLE", PinType::OUTPUT);
    self->addPin("PASS_B_ENABLE", PinType::OUTPUT);
    self->addPin("INVALID_OP", PinType::OUTPUT);
}

void defineALU8ResultSelectorPins(IOComponent* self) {
    self->addPin<8>("ARITHMETIC_RESULT", PinType::INPUT);
    self->addPin<8>("LOGIC_RESULT", PinType::INPUT);
    self->addPin<8>("SHIFT_RESULT", PinType::INPUT);
    self->addPin<8>("COMPARE_RESULT", PinType::INPUT);
    self->addPin<8>("A", PinType::INPUT);
    self->addPin<8>("B", PinType::INPUT);
    self->addPin("ARITHMETIC_ENABLE", PinType::INPUT);
    self->addPin("LOGIC_ENABLE", PinType::INPUT);
    self->addPin("SHIFT_ENABLE", PinType::INPUT);
    self->addPin("COMPARE_ENABLE", PinType::INPUT);
    self->addPin("PASS_A_ENABLE", PinType::INPUT);
    self->addPin("PASS_B_ENABLE", PinType::INPUT);
    self->addPin<8>("OUT", PinType::OUTPUT);
}

void defineALU8Pins(IOComponent* self) {
    self->addPin<8>("A", PinType::INPUT);
    self->addPin<8>("B", PinType::INPUT);
    self->addPin<5>("OP", PinType::INPUT);
    self->addPin("C_IN", PinType::INPUT);
    self->addPin<8>("OUT", PinType::OUTPUT);
    self->addPin("ZERO", PinType::OUTPUT);
    self->addPin("NEGATIVE", PinType::OUTPUT);
    self->addPin("CARRY", PinType::OUTPUT);
    self->addPin("OVERFLOW", PinType::OUTPUT);
    self->addPin("INVALID_OP", PinType::OUTPUT);
}

// Private implementation; public identity and pins belong to the family.
// One abstract delay tick, not the structural circuit's propagation timing.
template<class Structural>
class Behavioral8 final : public BasicComponent {
public:
    Behavioral8(std::string name, circuit::PinInitializer pins)
        : BasicComponent(std::move(name), 1, std::move(pins)) {}
    static constexpr const char* TypeName = Structural::TypeName;
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t time, Simulator& sim) override {
        if constexpr (std::is_same_v<Structural, Adder8>) {
            const auto r = value::add(read<8>("A"), read<8>("B"), bit("Cin"));
            write(sim, time, "Sum", r.out); write(sim, time, "Cout", r.carry);
        } else if constexpr (std::is_same_v<Structural, AddSub8>) {
            const auto r = value::addsub(read<8>("A"), read<8>("B"), bit("SUB"), bit("C_IN"));
            write(sim, time, "OUT", r.out); write(sim, time, "CARRY_OUT", r.carry);
            write(sim, time, "OVERFLOW", r.overflow);
        } else if constexpr (std::is_same_v<Structural, ArithmeticUnit8>) {
            const auto r = value::arithmetic(read<8>("A"), read<8>("B"), read<3>("OP"), bit("C_IN"));
            write(sim, time, "OUT", r.out); write(sim, time, "CARRY", r.carry);
            write(sim, time, "OVERFLOW", r.overflow); write(sim, time, "INVALID_OP", r.invalid);
        } else if constexpr (std::is_same_v<Structural, LogicUnit8>) {
            write(sim, time, "OUT", value::logic(read<8>("A"), read<8>("B"), read<3>("OP")));
        } else if constexpr (std::is_same_v<Structural, BarrelShifter8>) {
            const auto r = value::shift(read<8>("A"), read<8>("B"), read<3>("OP"), bit("C_IN"));
            write(sim, time, "OUT", r.out); write(sim, time, "CARRY", r.carry);
        } else if constexpr (std::is_same_v<Structural, ZeroDetect8>) {
            write(sim, time, "ZERO", value::zero(read<8>("A")));
        } else if constexpr (std::is_same_v<Structural, CompareSelect8>) {
            write(sim, time, "OUT", value::compare(read<8>("A"), read<8>("B"), read<8>("DIFF"),
                bit("CARRY_IN"), bit("OVERFLOW_IN"), read<3>("OP")));
        } else if constexpr (std::is_same_v<Structural, ALU8FlagUnit>) {
            writeFlags(sim, time, value::flags(read<8>("RESULT"), bit("ARITHMETIC"), bit("SHIFT"),
                bit("ARITH_CARRY"), bit("ARITH_OVERFLOW"), bit("SHIFT_CARRY")));
        } else if constexpr (std::is_same_v<Structural, ALU8ControlDecoder>) {
            const auto r = value::control(read<5>("OP"));
            write(sim, time, "LOCAL_OP", r.local); write(sim, time, "ARITH_OP", r.arithmetic);
            write(sim, time, "COMPARE_OP", r.compare);
            for (size_t i = 0; i < enable_names.size(); ++i) write(sim, time, enable_names[i], r.enables[i]);
            write(sim, time, "INVALID_OP", r.invalid);
        } else if constexpr (std::is_same_v<Structural, ALU8ResultSelector>) {
            value::Bits<6> enables;
            for (size_t i = 0; i < enable_names.size(); ++i) enables[i] = bit(enable_names[i]);
            write(sim, time, "OUT", value::select({read<8>("ARITHMETIC_RESULT"), read<8>("LOGIC_RESULT"),
                read<8>("SHIFT_RESULT"), read<8>("COMPARE_RESULT"), read<8>("A"), read<8>("B")}, enables));
        } else if constexpr (std::is_same_v<Structural, ALU8>) {
            const auto r = value::alu(read<8>("A"), read<8>("B"), read<5>("OP"), bit("C_IN"));
            write(sim, time, "OUT", r.out); writeFlags(sim, time, r.flags);
            write(sim, time, "INVALID_OP", r.invalid);
        } else {
            static_assert(!std::is_same_v<Structural, Structural>, "Missing behavioral evaluator");
        }
    }

private:
    static constexpr std::array<const char*, 6> enable_names{
        "ARITHMETIC_ENABLE", "LOGIC_ENABLE", "SHIFT_ENABLE", "COMPARE_ENABLE", "PASS_A_ENABLE", "PASS_B_ENABLE"};
    template<size_t Width>
    value::Bits<Width> read(const char* name) {
        const auto values = getInputPin<Width>(name)->getValueAsVector();
        value::Bits<Width> result;
        std::copy(values.begin(), values.end(), result.begin());
        return result;
    }
    LogicValue bit(const char* name) { return getInputPin(name)->getValue(); }
    template<size_t Width>
    void write(Simulator& sim, size_t time, const char* name, const value::Bits<Width>& values) {
        _updateOutputWire<Width>(sim, name, std::vector<LogicValue>(values.begin(), values.end()), time);
    }
    void write(Simulator& sim, size_t time, const char* name, LogicValue value) {
        _updateOutputWire(sim, name, value, time);
    }
    void writeFlags(Simulator& sim, size_t time, const value::Flags& flags) {
        write(sim, time, "ZERO", flags.zero); write(sim, time, "NEGATIVE", flags.negative);
        write(sim, time, "CARRY", flags.carry); write(sim, time, "OVERFLOW", flags.overflow);
    }
};

template<class Structural>
circuit::ComponentFamily makeFamily(std::string_view id, circuit::PinInitializer pins) {
    return {id, Structural::TypeName, pins,
        [](const std::string& name, const std::shared_ptr<circuit::BuildContext>& context) {
            return Component::createWithContext<Structural>(context, name);
        },
        [pins](const std::string& name, const std::shared_ptr<circuit::BuildContext>& context) {
            return Component::createWithContext<Behavioral8<Structural>>(context, name, pins);
        }};
}
} // namespace

namespace circuit::families {
const ComponentFamily Adder8 = makeFamily<::Adder8>("arithmetic.adder.width8", defineAdder8Pins);
const ComponentFamily AddSub8 = makeFamily<::AddSub8>("arithmetic.add-sub.width8", defineAddSub8Pins);
const ComponentFamily ArithmeticUnit8 = makeFamily<::ArithmeticUnit8>("arithmetic.unit.width8", defineArithmeticUnit8Pins);
const ComponentFamily LogicUnit8 = makeFamily<::LogicUnit8>("logic.unit.width8", defineLogicUnit8Pins);
const ComponentFamily BarrelShifter8 = makeFamily<::BarrelShifter8>("shift.barrel.width8", defineBarrelShifter8Pins);
const ComponentFamily ZeroDetect8 = makeFamily<::ZeroDetect8>("compare.zero.width8", defineZeroDetect8Pins);
const ComponentFamily CompareSelect8 = makeFamily<::CompareSelect8>("compare.select.width8", defineCompareSelect8Pins);
const ComponentFamily ALU8FlagUnit = makeFamily<::ALU8FlagUnit>("alu.flags.width8", defineALU8FlagUnitPins);
const ComponentFamily ALU8ControlDecoder = makeFamily<::ALU8ControlDecoder>("alu.control.width8", defineALU8ControlDecoderPins);
const ComponentFamily ALU8ResultSelector = makeFamily<::ALU8ResultSelector>("alu.result-selector.width8", defineALU8ResultSelectorPins);
const ComponentFamily ALU8 = makeFamily<::ALU8>("alu.width8", defineALU8Pins);
} // namespace circuit::families
