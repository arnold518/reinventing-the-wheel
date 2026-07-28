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

bool allKnown(const std::vector<LogicValue>& values) {
    for (const auto value : values) {
        if (value != LogicValue::LOW && value != LogicValue::HIGH) {
            return false;
        }
    }
    return true;
}

LogicValue logic(bool value) {
    return value ? LogicValue::HIGH : LogicValue::LOW;
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
        if (!allKnown(a_bits) || !allKnown(b_bits)
            || (sub_value != LogicValue::LOW
                && sub_value != LogicValue::HIGH)) {
            _updateOutputWire<32>(
                simulator,
                "OUT",
                std::vector<LogicValue>(32, LogicValue::UNKNOWN),
                current_time);
            _updateOutputWire(
                simulator, "CARRY_OUT", LogicValue::UNKNOWN, current_time);
            _updateOutputWire(
                simulator, "OVERFLOW", LogicValue::UNKNOWN, current_time);
            return;
        }

        const auto a = static_cast<uint32_t>(
            getInputPin<32>("A")->getValueAsUInt64());
        const auto b = static_cast<uint32_t>(
            getInputPin<32>("B")->getValueAsUInt64());
        const bool subtract = sub_value == LogicValue::HIGH;
        const uint32_t result = subtract ? a - b : a + b;

        const bool carry = subtract
            ? a >= b
            : (static_cast<uint64_t>(a) + static_cast<uint64_t>(b))
                > 0xffffffffULL;
        const bool overflow = subtract
            ? ((a ^ b) & (a ^ result) & 0x80000000U) != 0
            : (~(a ^ b) & (a ^ result) & 0x80000000U) != 0;

        _updateOutputWire<32>(simulator, "OUT", result, current_time);
        _updateOutputWire(
            simulator, "CARRY_OUT", logic(carry), current_time);
        _updateOutputWire(
            simulator, "OVERFLOW", logic(overflow), current_time);
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
