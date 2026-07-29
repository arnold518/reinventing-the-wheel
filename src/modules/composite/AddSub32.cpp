#include "modules/composite/AddSub32.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "simulator/Simulator.hpp"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {
void defineAddSub32Pins(IOComponent* self) {
    self->addPin<32>("A", PinType::INPUT);
    self->addPin<32>("B", PinType::INPUT);
    self->addPin("SUB", PinType::INPUT);
    self->addPin<32>("OUT", PinType::OUTPUT);
    self->addPin("CARRY_OUT", PinType::OUTPUT);
    self->addPin("OVERFLOW", PinType::OUTPUT);
}

bool known(LogicValue value) {
    return value == LogicValue::LOW || value == LogicValue::HIGH;
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

class AddSub32Direct final : public BasicComponent {
public:
    explicit AddSub32Direct(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::AddSub32.pinInitializer()) {}

    static constexpr const char* TypeName = "AddSub32";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const auto a_bits = getInputPin<32>("A")->getValueAsVector();
        const auto b_bits = getInputPin<32>("B")->getValueAsVector();
        const auto sub_value = getInputPin("SUB")->getValue();
        std::vector<LogicValue> result;
        result.reserve(32);
        auto carry = sub_value;
        auto carry_into_sign = LogicValue::UNKNOWN;
        for (size_t bit_index = 0; bit_index < 32; ++bit_index) {
            if (bit_index == 31) {
                carry_into_sign = carry;
            }
            const auto effective_b =
                logicXor(b_bits[bit_index], sub_value);
            const auto [sum, next_carry] =
                fullAdder(a_bits[bit_index], effective_b, carry);
            result.push_back(sum);
            carry = next_carry;
        }

        _updateOutputWire<32>(
            simulator, "OUT", result, current_time);
        _updateOutputWire(
            simulator, "CARRY_OUT", carry, current_time);
        _updateOutputWire(
            simulator,
            "OVERFLOW",
            logicXor(carry_into_sign, carry),
            current_time);
    }
};
}

namespace circuit::families {
const ComponentFamily AddSub32{
    "arithmetic.add-sub.width32",
    "AddSub32",
    defineAddSub32Pins,
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::AddSub32>(context, name);
    },
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<AddSub32Direct>(context, name);
    }};
}

AddSub32::AddSub32(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::AddSub32.pinInitializer()) {}

void AddSub32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<32>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<32>>("OUT_JOIN");
    builder.addNewComponent<XORGate>("OVERFLOW_XOR");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<BitSplitter<32>, 32>("A_SPLIT", "IN")});
    builder.addNewWire<32>(
        "B_bus_internal",
        getInputPin<32>("B"),
        {builder.getInputPin<BitSplitter<32>, 32>("B_SPLIT", "IN")});

    std::vector<std::shared_ptr<Pin<>>> sub_sinks;
    sub_sinks.reserve(33);

    for (size_t i = 0; i < 32; ++i) {
        const auto bit = std::to_string(i);
        const auto xor_name = "B_XOR_SUB_" + bit;
        const auto adder_name = "FA" + bit;

        builder.addNewComponent<XORGate>(xor_name);
        builder.addNewComponent<FullAdder>(adder_name);

        sub_sinks.push_back(builder.getInputPin<XORGate>(xor_name, "B"));

        builder.addNewWire(
            "A_bit_" + bit,
            builder.getOutputPin<BitSplitter<32>>("A_SPLIT", "OUT_" + bit),
            {builder.getInputPin<FullAdder>(adder_name, "A")});
        builder.addNewWire(
            "B_bit_" + bit,
            builder.getOutputPin<BitSplitter<32>>("B_SPLIT", "OUT_" + bit),
            {builder.getInputPin<XORGate>(xor_name, "A")});
        builder.addNewWire(
            "B_xor_SUB_" + bit + "_to_FA",
            builder.getOutputPin<XORGate>(xor_name, "OUT"),
            {builder.getInputPin<FullAdder>(adder_name, "B")});
        builder.addNewWire(
            "SUM_bit_" + bit,
            builder.getOutputPin<FullAdder>(adder_name, "Sum"),
            {builder.getInputPin<BitJoiner<32>>("OUT_JOIN", "IN_" + bit)});
    }

    sub_sinks.push_back(builder.getInputPin<FullAdder>("FA0", "Carry_in"));
    builder.addNewWire("SUB_control", getInputPin("SUB"), sub_sinks);

    for (size_t i = 0; i < 31; ++i) {
        const auto from = std::to_string(i);
        const auto to = std::to_string(i + 1);
        std::vector<std::shared_ptr<Pin<>>> sinks{
            builder.getInputPin<FullAdder>("FA" + to, "Carry_in")};
        if (i == 30) {
            sinks.push_back(builder.getInputPin<XORGate>("OVERFLOW_XOR", "A"));
        }
        builder.addNewWire(
            "carry_" + from + "_to_" + to,
            builder.getOutputPin<FullAdder>("FA" + from, "Carry_out"),
            sinks);
    }

    builder.addNewWire(
        "carry_out_fanout",
        builder.getOutputPin<FullAdder>("FA31", "Carry_out"),
        {getOutputPin("CARRY_OUT"),
         builder.getInputPin<XORGate>("OVERFLOW_XOR", "B")});
    builder.addNewWire(
        "overflow_to_output",
        builder.getOutputPin<XORGate>("OVERFLOW_XOR", "OUT"),
        {getOutputPin("OVERFLOW")});
    builder.addNewWire<32>(
        "out_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("OUT_JOIN", "OUT"),
        {getOutputPin<32>("OUT")});
}
