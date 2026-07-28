#include "modules/composite/Logic32.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "simulator/Simulator.hpp"
#include <string>
#include <utility>
#include <vector>

namespace {
void defineLogic32Pins(IOComponent* self) {
    self->addPin<32>("A", PinType::INPUT);
    self->addPin<32>("B", PinType::INPUT);
    self->addPin<32>("AND_OUT", PinType::OUTPUT);
    self->addPin<32>("OR_OUT", PinType::OUTPUT);
    self->addPin<32>("XOR_OUT", PinType::OUTPUT);
}

bool known(LogicValue value) {
    return value == LogicValue::LOW || value == LogicValue::HIGH;
}

LogicValue andValue(LogicValue left, LogicValue right) {
    if (left == LogicValue::LOW || right == LogicValue::LOW) {
        return LogicValue::LOW;
    }
    if (left == LogicValue::HIGH && right == LogicValue::HIGH) {
        return LogicValue::HIGH;
    }
    return LogicValue::UNKNOWN;
}

LogicValue orValue(LogicValue left, LogicValue right) {
    if (left == LogicValue::HIGH || right == LogicValue::HIGH) {
        return LogicValue::HIGH;
    }
    if (left == LogicValue::LOW && right == LogicValue::LOW) {
        return LogicValue::LOW;
    }
    return LogicValue::UNKNOWN;
}

LogicValue xorValue(LogicValue left, LogicValue right) {
    if (!known(left) || !known(right)) {
        return LogicValue::UNKNOWN;
    }
    return left == right ? LogicValue::LOW : LogicValue::HIGH;
}

class Logic32Direct final : public BasicComponent {
public:
    explicit Logic32Direct(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::Logic32.pinInitializer()) {}

    static constexpr const char* TypeName = "Logic32";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const auto a = getInputPin<32>("A")->getValueAsVector();
        const auto b = getInputPin<32>("B")->getValueAsVector();
        std::vector<LogicValue> and_result(32, LogicValue::UNKNOWN);
        std::vector<LogicValue> or_result(32, LogicValue::UNKNOWN);
        std::vector<LogicValue> xor_result(32, LogicValue::UNKNOWN);
        for (size_t bit = 0; bit < 32; ++bit) {
            and_result[bit] = andValue(a[bit], b[bit]);
            or_result[bit] = orValue(a[bit], b[bit]);
            xor_result[bit] = xorValue(a[bit], b[bit]);
        }
        _updateOutputWire<32>(
            simulator, "AND_OUT", and_result, current_time);
        _updateOutputWire<32>(
            simulator, "OR_OUT", or_result, current_time);
        _updateOutputWire<32>(
            simulator, "XOR_OUT", xor_result, current_time);
    }
};
}

namespace circuit::families {
const ComponentFamily Logic32{
    "logic.combined.width32",
    "Logic32",
    defineLogic32Pins,
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::Logic32>(context, name);
    },
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<Logic32Direct>(context, name);
    }};
}

Logic32::Logic32(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::Logic32.pinInitializer()) {}

void Logic32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<32>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<32>>("AND_JOIN");
    builder.addNewComponent<BitJoiner<32>>("OR_JOIN");
    builder.addNewComponent<BitJoiner<32>>("XOR_JOIN");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<BitSplitter<32>, 32>("A_SPLIT", "IN")});
    builder.addNewWire<32>(
        "B_bus_internal",
        getInputPin<32>("B"),
        {builder.getInputPin<BitSplitter<32>, 32>("B_SPLIT", "IN")});

    for (size_t i = 0; i < 32; ++i) {
        const auto bit = std::to_string(i);
        const auto and_name = "AND_" + bit;
        const auto or_name = "OR_" + bit;
        const auto xor_name = "XOR_" + bit;

        builder.addNewComponent<ANDGate>(and_name);
        builder.addNewComponent<ORGate>(or_name);
        builder.addNewComponent<XORGate>(xor_name);

        builder.addNewWire(
            "A_bit_" + bit,
            builder.getOutputPin<BitSplitter<32>>("A_SPLIT", "OUT_" + bit),
            {builder.getInputPin<ANDGate>(and_name, "A"),
             builder.getInputPin<ORGate>(or_name, "A"),
             builder.getInputPin<XORGate>(xor_name, "A")});
        builder.addNewWire(
            "B_bit_" + bit,
            builder.getOutputPin<BitSplitter<32>>("B_SPLIT", "OUT_" + bit),
            {builder.getInputPin<ANDGate>(and_name, "B"),
             builder.getInputPin<ORGate>(or_name, "B"),
             builder.getInputPin<XORGate>(xor_name, "B")});
        builder.addNewWire(
            "AND_bit_" + bit,
            builder.getOutputPin<ANDGate>(and_name, "OUT"),
            {builder.getInputPin<BitJoiner<32>>("AND_JOIN", "IN_" + bit)});
        builder.addNewWire(
            "OR_bit_" + bit,
            builder.getOutputPin<ORGate>(or_name, "OUT"),
            {builder.getInputPin<BitJoiner<32>>("OR_JOIN", "IN_" + bit)});
        builder.addNewWire(
            "XOR_bit_" + bit,
            builder.getOutputPin<XORGate>(xor_name, "OUT"),
            {builder.getInputPin<BitJoiner<32>>("XOR_JOIN", "IN_" + bit)});
    }

    builder.addNewWire<32>(
        "AND_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("AND_JOIN", "OUT"),
        {getOutputPin<32>("AND_OUT")});
    builder.addNewWire<32>(
        "OR_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("OR_JOIN", "OUT"),
        {getOutputPin<32>("OR_OUT")});
    builder.addNewWire<32>(
        "XOR_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("XOR_JOIN", "OUT"),
        {getOutputPin<32>("XOR_OUT")});
}
