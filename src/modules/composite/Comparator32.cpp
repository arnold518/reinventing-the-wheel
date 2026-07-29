#include "modules/composite/Comparator32.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/ZeroDetect32.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "simulator/Simulator.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace {
void defineComparator32Pins(IOComponent* self) {
    self->addPin<32>("A", PinType::INPUT);
    self->addPin<32>("B", PinType::INPUT);
    self->addPin("EQ", PinType::OUTPUT);
    self->addPin("LT_SIGNED", PinType::OUTPUT);
    self->addPin("LT_UNSIGNED", PinType::OUTPUT);
    self->addPin<32>("DIFF", PinType::OUTPUT);
    self->addPin("CARRY_OUT", PinType::OUTPUT);
    self->addPin("OVERFLOW", PinType::OUTPUT);
}

bool known(LogicValue value) {
    return value == LogicValue::LOW || value == LogicValue::HIGH;
}

LogicValue logicNot(LogicValue value) {
    if (value == LogicValue::LOW) {
        return LogicValue::HIGH;
    }
    if (value == LogicValue::HIGH) {
        return LogicValue::LOW;
    }
    return LogicValue::UNKNOWN;
}

LogicValue logicAnd(LogicValue left, LogicValue right) {
    if (left == LogicValue::LOW || right == LogicValue::LOW) {
        return LogicValue::LOW;
    }
    if (left == LogicValue::HIGH && right == LogicValue::HIGH) {
        return LogicValue::HIGH;
    }
    return LogicValue::UNKNOWN;
}

LogicValue logicOr(LogicValue left, LogicValue right) {
    if (left == LogicValue::HIGH || right == LogicValue::HIGH) {
        return LogicValue::HIGH;
    }
    if (left == LogicValue::LOW && right == LogicValue::LOW) {
        return LogicValue::LOW;
    }
    return LogicValue::UNKNOWN;
}

LogicValue logicXor(LogicValue left, LogicValue right) {
    if (!known(left) || !known(right)) {
        return LogicValue::UNKNOWN;
    }
    return left == right ? LogicValue::LOW : LogicValue::HIGH;
}

std::pair<LogicValue, LogicValue> fullAdder(
    LogicValue left,
    LogicValue right,
    LogicValue carry_in) {
    const auto first_sum = logicXor(left, right);
    const auto first_carry = logicAnd(left, right);
    const auto sum = logicXor(first_sum, carry_in);
    const auto second_carry = logicAnd(first_sum, carry_in);
    return {sum, logicOr(first_carry, second_carry)};
}

LogicValue zeroDetect(const std::vector<LogicValue>& values) {
    bool has_unknown = false;
    for (const auto value : values) {
        if (value == LogicValue::HIGH) {
            return LogicValue::LOW;
        }
        if (value != LogicValue::LOW) {
            has_unknown = true;
        }
    }
    return has_unknown ? LogicValue::UNKNOWN : LogicValue::HIGH;
}

class Comparator32Direct final : public BasicComponent {
public:
    explicit Comparator32Direct(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::Comparator32.pinInitializer()) {}

    static constexpr const char* TypeName = "Comparator32";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const auto a_bits = getInputPin<32>("A")->getValueAsVector();
        const auto b_bits = getInputPin<32>("B")->getValueAsVector();
        std::vector<LogicValue> difference;
        difference.reserve(32);
        auto carry = LogicValue::HIGH;
        auto carry_into_sign = LogicValue::UNKNOWN;
        for (size_t bit_index = 0; bit_index < 32; ++bit_index) {
            if (bit_index == 31) {
                carry_into_sign = carry;
            }
            const auto effective_b =
                logicXor(b_bits[bit_index], LogicValue::HIGH);
            const auto [sum, next_carry] =
                fullAdder(a_bits[bit_index], effective_b, carry);
            difference.push_back(sum);
            carry = next_carry;
        }
        const auto overflow = logicXor(carry_into_sign, carry);

        _updateOutputWire<32>(
            simulator, "DIFF", difference, current_time);
        _updateOutputWire(
            simulator, "EQ", zeroDetect(difference), current_time);
        _updateOutputWire(
            simulator,
            "LT_SIGNED",
            logicXor(difference[31], overflow),
            current_time);
        _updateOutputWire(
            simulator,
            "LT_UNSIGNED",
            logicNot(carry),
            current_time);
        _updateOutputWire(
            simulator, "CARRY_OUT", carry, current_time);
        _updateOutputWire(
            simulator, "OVERFLOW", overflow, current_time);
    }
};
}

namespace circuit::families {
const ComponentFamily Comparator32{
    "compare.width32",
    "Comparator32",
    defineComparator32Pins,
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::Comparator32>(context, name);
    },
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<Comparator32Direct>(context, name);
    }};
}

Comparator32::Comparator32(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::Comparator32.pinInitializer()) {}

void Comparator32::buildInternals(ComponentBuilder& builder) {
    builder.add(circuit::families::AddSub32, "SUB");
    builder.add(circuit::families::ZeroDetect32, "ZERO_DETECT");
    builder.addNewComponent<BitSplitter<32>>("DIFF_SPLIT");
    builder.addNewComponent<ConstantValue<1>>("CONST_HIGH", 1);
    builder.addNewComponent<NOTGate>("NOT_CARRY");
    builder.addNewComponent<XORGate>("SIGNED_LT_XOR");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<AddSub32, 32>("SUB", "A")});
    builder.addNewWire<32>(
        "B_bus_internal",
        getInputPin<32>("B"),
        {builder.getInputPin<AddSub32, 32>("SUB", "B")});
    builder.addNewWire(
        "CONST_HIGH_to_SUB",
        builder.getOutputPin<ConstantValue<1>>("CONST_HIGH", "OUT"),
        {builder.getInputPin<AddSub32>("SUB", "SUB")});

    builder.addNewWire<32>(
        "diff_bus_fanout",
        builder.getOutputPin<AddSub32, 32>("SUB", "OUT"),
        {getOutputPin<32>("DIFF"),
         builder.getInputPin<ZeroDetect32, 32>("ZERO_DETECT", "A"),
         builder.getInputPin<BitSplitter<32>, 32>("DIFF_SPLIT", "IN")});
    builder.addNewWire(
        "zero_detect_to_EQ",
        builder.getOutputPin<ZeroDetect32>("ZERO_DETECT", "ZERO"),
        {getOutputPin("EQ")});
    builder.addNewWire(
        "carry_out_fanout",
        builder.getOutputPin<AddSub32>("SUB", "CARRY_OUT"),
        {getOutputPin("CARRY_OUT"),
         builder.getInputPin<NOTGate>("NOT_CARRY", "IN")});
    builder.addNewWire(
        "not_carry_to_LT_UNSIGNED",
        builder.getOutputPin<NOTGate>("NOT_CARRY", "OUT"),
        {getOutputPin("LT_UNSIGNED")});
    builder.addNewWire(
        "diff_sign_to_signed_lt",
        builder.getOutputPin<BitSplitter<32>>("DIFF_SPLIT", "OUT_31"),
        {builder.getInputPin<XORGate>("SIGNED_LT_XOR", "A")});
    builder.addNewWire(
        "overflow_fanout",
        builder.getOutputPin<AddSub32>("SUB", "OVERFLOW"),
        {getOutputPin("OVERFLOW"),
         builder.getInputPin<XORGate>("SIGNED_LT_XOR", "B")});
    builder.addNewWire(
        "signed_lt_to_output",
        builder.getOutputPin<XORGate>("SIGNED_LT_XOR", "OUT"),
        {getOutputPin("LT_SIGNED")});
}
