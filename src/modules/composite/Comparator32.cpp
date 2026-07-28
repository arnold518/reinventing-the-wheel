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
        if (!allKnown(a_bits) || !allKnown(b_bits)) {
            _updateOutputWire<32>(
                simulator,
                "DIFF",
                std::vector<LogicValue>(32, LogicValue::UNKNOWN),
                current_time);
            for (const char* pin :
                 {"EQ", "LT_SIGNED", "LT_UNSIGNED",
                  "CARRY_OUT", "OVERFLOW"}) {
                _updateOutputWire(
                    simulator, pin, LogicValue::UNKNOWN, current_time);
            }
            return;
        }

        const auto a = static_cast<uint32_t>(
            getInputPin<32>("A")->getValueAsUInt64());
        const auto b = static_cast<uint32_t>(
            getInputPin<32>("B")->getValueAsUInt64());
        const uint32_t difference = a - b;
        const bool overflow =
            ((a ^ b) & (a ^ difference) & 0x80000000U) != 0;

        _updateOutputWire<32>(
            simulator, "DIFF", difference, current_time);
        _updateOutputWire(
            simulator, "EQ", logic(a == b), current_time);
        _updateOutputWire(
            simulator,
            "LT_SIGNED",
            logic(static_cast<int32_t>(a) < static_cast<int32_t>(b)),
            current_time);
        _updateOutputWire(
            simulator, "LT_UNSIGNED", logic(a < b), current_time);
        _updateOutputWire(
            simulator, "CARRY_OUT", logic(a >= b), current_time);
        _updateOutputWire(
            simulator, "OVERFLOW", logic(overflow), current_time);
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
