#include "modules/composite/ZeroDetect32.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "simulator/Simulator.hpp"
#include <string>
#include <utility>

namespace {
void defineZeroDetect32Pins(IOComponent* self) {
    self->addPin<32>("A", PinType::INPUT);
    self->addPin("ZERO", PinType::OUTPUT);
}

class ZeroDetect32Direct final : public BasicComponent {
public:
    explicit ZeroDetect32Direct(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::ZeroDetect32.pinInitializer()) {}

    static constexpr const char* TypeName = "ZeroDetect32";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        bool has_unknown = false;
        for (const auto value :
             getInputPin<32>("A")->getValueAsVector()) {
            if (value == LogicValue::HIGH) {
                _updateOutputWire(
                    simulator, "ZERO", LogicValue::LOW, current_time);
                return;
            }
            if (value != LogicValue::LOW) {
                has_unknown = true;
            }
        }
        _updateOutputWire(
            simulator,
            "ZERO",
            has_unknown ? LogicValue::UNKNOWN : LogicValue::HIGH,
            current_time);
    }
};
}

namespace circuit::families {
const ComponentFamily ZeroDetect32{
    "compare.zero.width32",
    "ZeroDetect32",
    defineZeroDetect32Pins,
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::ZeroDetect32>(context, name);
    },
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<ZeroDetect32Direct>(context, name);
    }};
}

ZeroDetect32::ZeroDetect32(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::ZeroDetect32.pinInitializer()) {}

void ZeroDetect32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("A_SPLIT");
    builder.addNewComponent<ORGate>("OR_1");
    builder.addNewComponent<NOTGate>("NOT_ANY");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<BitSplitter<32>, 32>("A_SPLIT", "IN")});

    builder.addNewWire(
        "A_bit_0_to_OR_1",
        builder.getOutputPin<BitSplitter<32>>("A_SPLIT", "OUT_0"),
        {builder.getInputPin<ORGate>("OR_1", "A")});
    builder.addNewWire(
        "A_bit_1_to_OR_1",
        builder.getOutputPin<BitSplitter<32>>("A_SPLIT", "OUT_1"),
        {builder.getInputPin<ORGate>("OR_1", "B")});

    for (size_t i = 2; i < 32; ++i) {
        const auto index = std::to_string(i);
        const auto prev = std::to_string(i - 1);
        const auto gate_name = "OR_" + index;
        builder.addNewComponent<ORGate>(gate_name);
        builder.addNewWire(
            "OR_" + prev + "_to_" + gate_name,
            builder.getOutputPin<ORGate>("OR_" + prev, "OUT"),
            {builder.getInputPin<ORGate>(gate_name, "A")});
        builder.addNewWire(
            "A_bit_" + index + "_to_" + gate_name,
            builder.getOutputPin<BitSplitter<32>>("A_SPLIT", "OUT_" + index),
            {builder.getInputPin<ORGate>(gate_name, "B")});
    }

    builder.addNewWire(
        "any_bit_set_to_NOT",
        builder.getOutputPin<ORGate>("OR_31", "OUT"),
        {builder.getInputPin<NOTGate>("NOT_ANY", "IN")});
    builder.addNewWire(
        "not_any_to_ZERO",
        builder.getOutputPin<NOTGate>("NOT_ANY", "OUT"),
        {getOutputPin("ZERO")});
}
