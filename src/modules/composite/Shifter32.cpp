#include "modules/composite/Shifter32.hpp"
#include "components/BasicComponent.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include "simulator/Simulator.hpp"
#include <array>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {
void defineShifter32Pins(IOComponent* self) {
    self->addPin<32>("A", PinType::INPUT);
    self->addPin<32>("B", PinType::INPUT);
    self->addPin("LEFT", PinType::INPUT);
    self->addPin("ARITHMETIC", PinType::INPUT);
    self->addPin<32>("OUT", PinType::OUTPUT);
}

using OneBitPin = std::shared_ptr<Pin<>>;
using SinkList = std::vector<OneBitPin>;

LogicValue logicNot(LogicValue value) {
    if (value == LogicValue::LOW) return LogicValue::HIGH;
    if (value == LogicValue::HIGH) return LogicValue::LOW;
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

LogicValue muxValue(
    LogicValue when_low,
    LogicValue when_high,
    LogicValue select) {
    return logicOr(
        logicAnd(when_low, logicNot(select)),
        logicAnd(when_high, select));
}

class Shifter32Direct final : public BasicComponent {
public:
    explicit Shifter32Direct(std::string name)
        : BasicComponent(
              std::move(name),
              1,
              circuit::families::Shifter32.pinInitializer()) {}

    static constexpr const char* TypeName = "Shifter32";
    const char* getTypeName() const override { return TypeName; }

    void evaluate(size_t current_time, Simulator& simulator) override {
        const auto a = getInputPin<32>("A")->getValueAsVector();
        const auto b = getInputPin<32>("B")->getValueAsVector();
        const auto left = getInputValue("LEFT");
        const auto arithmetic = getInputValue("ARITHMETIC");

        // A left shift is implemented by reversing the word before and after
        // one shared right-shift network. LEFT also disables sign fill.
        std::vector<LogicValue> shifted(32, LogicValue::UNKNOWN);
        for (size_t bit = 0; bit < 32; ++bit) {
            shifted[bit] = muxValue(a[bit], a[31 - bit], left);
        }
        const auto fill = logicAnd(
            logicAnd(a[31], arithmetic), logicNot(left));
        for (size_t stage = 0; stage < 5; ++stage) {
            const size_t amount = size_t{1} << stage;
            auto next = shifted;
            for (size_t bit = 0; bit < 32; ++bit) {
                const auto moved = bit + amount < 32
                    ? shifted[bit + amount]
                    : fill;
                next[bit] = muxValue(shifted[bit], moved, b[stage]);
            }
            shifted = std::move(next);
        }

        std::vector<LogicValue> result(32, LogicValue::UNKNOWN);
        for (size_t bit = 0; bit < 32; ++bit) {
            result[bit] = muxValue(
                shifted[bit], shifted[31 - bit], left);
        }
        _updateOutputWire<32>(
            simulator, "OUT", result, current_time);
    }
};
} // namespace

namespace circuit::families {
const ComponentFamily Shifter32{
    "shift.barrel.width32",
    "Shifter32",
    defineShifter32Pins,
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::Shifter32>(context, name);
    },
    [](const std::string& name,
       const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<Shifter32Direct>(context, name);
    }};
}

Shifter32::Shifter32(std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::Shifter32.pinInitializer()) {}

void Shifter32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<32>>("B_SPLIT");
    builder.addNewComponent<BitJoiner<32>>("OUT_JOIN");
    builder.addNewComponent<NOTGate>("NOT_LEFT");
    builder.addNewComponent<ANDGate>("ARITHMETIC_SIGN");
    builder.addNewComponent<ANDGate>("FILL_ENABLE");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<BitSplitter<32>, 32>("A_SPLIT", "IN")});
    builder.addNewWire<32>(
        "B_bus_internal",
        getInputPin<32>("B"),
        {builder.getInputPin<BitSplitter<32>, 32>("B_SPLIT", "IN")});
    builder.addNewWire(
        "ARITHMETIC_to_fill",
        getInputPin("ARITHMETIC"),
        {builder.getInputPin<ANDGate>("ARITHMETIC_SIGN", "B")});
    builder.addNewWire(
        "ARITHMETIC_SIGN_to_enable",
        builder.getOutputPin<ANDGate>("ARITHMETIC_SIGN", "OUT"),
        {builder.getInputPin<ANDGate>("FILL_ENABLE", "A")});
    builder.addNewWire(
        "NOT_LEFT_to_fill",
        builder.getOutputPin<NOTGate>("NOT_LEFT", "OUT"),
        {builder.getInputPin<ANDGate>("FILL_ENABLE", "B")});

    // Reverse the input for SLL, feed one five-stage right barrel shifter,
    // then reverse the result back. This is the shared hardware resource.
    std::array<SinkList, 32> a_sinks;
    a_sinks[31].push_back(
        builder.getInputPin<ANDGate>("ARITHMETIC_SIGN", "A"));
    std::vector<OneBitPin> current_sources;
    current_sources.reserve(32);
    std::vector<OneBitPin> left_select_sinks;
    left_select_sinks.reserve(64);
    left_select_sinks.push_back(
        builder.getInputPin<NOTGate>("NOT_LEFT", "IN"));
    for (size_t bit = 0; bit < 32; ++bit) {
        const auto mux = "INPUT_REVERSE_" + std::to_string(bit);
        builder.addNewComponent<Mux2to1>(mux);
        a_sinks[bit].push_back(
            builder.getInputPin<Mux2to1>(mux, "A"));
        a_sinks[31 - bit].push_back(
            builder.getInputPin<Mux2to1>(mux, "B"));
        left_select_sinks.push_back(
            builder.getInputPin<Mux2to1>(mux, "SEL"));
        current_sources.push_back(
            builder.getOutputPin<Mux2to1>(mux, "OUT"));
    }

    std::array<SinkList, 5> amount_select_sinks;
    SinkList fill_sinks;
    for (size_t stage = 0; stage < 5; ++stage) {
        const size_t amount = size_t{1} << stage;
        std::array<SinkList, 32> current_sinks;
        std::vector<OneBitPin> next_sources;
        next_sources.reserve(32);
        for (size_t bit = 0; bit < 32; ++bit) {
            const auto mux = "SHIFT_S" + std::to_string(stage)
                + "_B" + std::to_string(bit);
            builder.addNewComponent<Mux2to1>(mux);
            current_sinks[bit].push_back(
                builder.getInputPin<Mux2to1>(mux, "A"));
            if (bit + amount < 32) {
                current_sinks[bit + amount].push_back(
                    builder.getInputPin<Mux2to1>(mux, "B"));
            } else {
                fill_sinks.push_back(
                    builder.getInputPin<Mux2to1>(mux, "B"));
            }
            amount_select_sinks[stage].push_back(
                builder.getInputPin<Mux2to1>(mux, "SEL"));
            next_sources.push_back(
                builder.getOutputPin<Mux2to1>(mux, "OUT"));
        }
        for (size_t bit = 0; bit < 32; ++bit) {
            builder.addNewWire(
                "SHIFT_S" + std::to_string(stage)
                    + "_SRC_" + std::to_string(bit),
                current_sources[bit],
                current_sinks[bit]);
        }
        current_sources = std::move(next_sources);
    }

    std::array<SinkList, 32> output_sinks;
    for (size_t bit = 0; bit < 32; ++bit) {
        const auto mux = "OUTPUT_REVERSE_" + std::to_string(bit);
        builder.addNewComponent<Mux2to1>(mux);
        output_sinks[bit].push_back(
            builder.getInputPin<Mux2to1>(mux, "A"));
        output_sinks[31 - bit].push_back(
            builder.getInputPin<Mux2to1>(mux, "B"));
        left_select_sinks.push_back(
            builder.getInputPin<Mux2to1>(mux, "SEL"));
        builder.addNewWire(
            "SHIFT_RESULT_" + std::to_string(bit),
            builder.getOutputPin<Mux2to1>(mux, "OUT"),
            {builder.getInputPin<BitJoiner<32>>(
                "OUT_JOIN", "IN_" + std::to_string(bit))});
    }
    for (size_t bit = 0; bit < 32; ++bit) {
        builder.addNewWire(
            "SHIFT_OUT_SOURCE_" + std::to_string(bit),
            current_sources[bit],
            output_sinks[bit]);
    }

    for (size_t bit = 0; bit < 32; ++bit) {
        builder.addNewWire(
            "A_bit_" + std::to_string(bit),
            builder.getOutputPin<BitSplitter<32>>(
                "A_SPLIT", "OUT_" + std::to_string(bit)),
            a_sinks[bit]);
    }
    for (size_t bit = 0; bit < 5; ++bit) {
        builder.addNewWire(
            "B_shift_bit_" + std::to_string(bit),
            builder.getOutputPin<BitSplitter<32>>(
                "B_SPLIT", "OUT_" + std::to_string(bit)),
            amount_select_sinks[bit]);
    }
    builder.addNewWire(
        "LEFT_fanout", getInputPin("LEFT"), left_select_sinks);
    builder.addNewWire(
        "FILL_fanout",
        builder.getOutputPin<ANDGate>("FILL_ENABLE", "OUT"),
        fill_sinks);
    builder.addNewWire<32>(
        "OUT_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("OUT_JOIN", "OUT"),
        {getOutputPin<32>("OUT")});
}
