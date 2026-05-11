#include "modules/composite/Shifter32.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace {
enum class ShiftMode {
    Left,
    RightLogical,
    RightArithmetic,
};

using OneBitPin = std::shared_ptr<Pin<>>;
using SinkList = std::vector<OneBitPin>;

void buildShiftNetwork(
    ComponentBuilder& builder,
    const std::string& prefix,
    ShiftMode mode,
    const std::string& output_joiner,
    std::array<SinkList, 32>& a_sinks,
    std::array<SinkList, 5>& select_sinks,
    SinkList& zero_sinks) {

    std::vector<OneBitPin> current_sources;
    current_sources.reserve(32);
    bool current_is_input_a = true;

    for (size_t stage = 0; stage < 5; ++stage) {
        const size_t amount = size_t{1} << stage;
        std::array<SinkList, 32> current_sinks;
        std::vector<OneBitPin> next_sources;
        next_sources.reserve(32);

        auto addCurrentSink = [&](size_t bit, OneBitPin sink) {
            if (current_is_input_a) {
                a_sinks[bit].push_back(std::move(sink));
            } else {
                current_sinks[bit].push_back(std::move(sink));
            }
        };

        for (size_t bit = 0; bit < 32; ++bit) {
            const auto bit_text = std::to_string(bit);
            const auto mux_name = prefix + "_S" + std::to_string(stage) + "_B" + bit_text;
            builder.addNewComponent<Mux2to1>(mux_name);

            select_sinks[stage].push_back(builder.getInputPin<Mux2to1>(mux_name, "SEL"));
            addCurrentSink(bit, builder.getInputPin<Mux2to1>(mux_name, "A"));

            if (mode == ShiftMode::Left) {
                if (bit >= amount) {
                    addCurrentSink(bit - amount, builder.getInputPin<Mux2to1>(mux_name, "B"));
                } else {
                    zero_sinks.push_back(builder.getInputPin<Mux2to1>(mux_name, "B"));
                }
            } else {
                const size_t source_bit = bit + amount;
                if (source_bit < 32) {
                    addCurrentSink(source_bit, builder.getInputPin<Mux2to1>(mux_name, "B"));
                } else if (mode == ShiftMode::RightArithmetic) {
                    a_sinks[31].push_back(builder.getInputPin<Mux2to1>(mux_name, "B"));
                } else {
                    zero_sinks.push_back(builder.getInputPin<Mux2to1>(mux_name, "B"));
                }
            }

            next_sources.push_back(builder.getOutputPin<Mux2to1>(mux_name, "OUT"));
        }

        if (!current_is_input_a) {
            for (size_t bit = 0; bit < 32; ++bit) {
                builder.addNewWire(
                    prefix + "_S" + std::to_string(stage) + "_SRC_" + std::to_string(bit),
                    current_sources[bit],
                    current_sinks[bit]);
            }
        }

        current_sources = std::move(next_sources);
        current_is_input_a = false;
    }

    for (size_t bit = 0; bit < 32; ++bit) {
        builder.addNewWire(
            prefix + "_OUT_" + std::to_string(bit),
            current_sources[bit],
            {builder.getInputPin<BitJoiner<32>>(output_joiner, "IN_" + std::to_string(bit))});
    }
}
}

Shifter32::Shifter32(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
          self->addPin<32>("A", PinType::INPUT);
          self->addPin<32>("B", PinType::INPUT);
          self->addPin<32>("SLL_OUT", PinType::OUTPUT);
          self->addPin<32>("SRL_OUT", PinType::OUTPUT);
          self->addPin<32>("SRA_OUT", PinType::OUTPUT);
      }) {}

void Shifter32::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("A_SPLIT");
    builder.addNewComponent<BitSplitter<32>>("B_SPLIT");
    builder.addNewComponent<ConstantValue<1, 32>>("CONST_LOW", 0);
    builder.addNewComponent<BitJoiner<32>>("SLL_JOIN");
    builder.addNewComponent<BitJoiner<32>>("SRL_JOIN");
    builder.addNewComponent<BitJoiner<32>>("SRA_JOIN");

    builder.addNewWire<32>(
        "A_bus_internal",
        getInputPin<32>("A"),
        {builder.getInputPin<BitSplitter<32>, 32>("A_SPLIT", "IN"),
         builder.getInputPin<ConstantValue<1, 32>, 32>("CONST_LOW", "TRIGGER")});
    builder.addNewWire<32>(
        "B_bus_internal",
        getInputPin<32>("B"),
        {builder.getInputPin<BitSplitter<32>, 32>("B_SPLIT", "IN")});

    std::array<SinkList, 32> a_sinks;
    std::array<SinkList, 5> select_sinks;
    SinkList zero_sinks;

    buildShiftNetwork(builder, "SLL", ShiftMode::Left, "SLL_JOIN", a_sinks, select_sinks, zero_sinks);
    buildShiftNetwork(builder, "SRL", ShiftMode::RightLogical, "SRL_JOIN", a_sinks, select_sinks, zero_sinks);
    buildShiftNetwork(builder, "SRA", ShiftMode::RightArithmetic, "SRA_JOIN", a_sinks, select_sinks, zero_sinks);

    for (size_t bit = 0; bit < 32; ++bit) {
        builder.addNewWire(
            "A_bit_" + std::to_string(bit),
            builder.getOutputPin<BitSplitter<32>>("A_SPLIT", "OUT_" + std::to_string(bit)),
            a_sinks[bit]);
    }

    for (size_t bit = 0; bit < 5; ++bit) {
        builder.addNewWire(
            "B_shift_bit_" + std::to_string(bit),
            builder.getOutputPin<BitSplitter<32>>("B_SPLIT", "OUT_" + std::to_string(bit)),
            select_sinks[bit]);
    }

    builder.addNewWire(
        "CONST_LOW_to_zero_fill",
        builder.getOutputPin<ConstantValue<1, 32>>("CONST_LOW", "OUT"),
        zero_sinks);

    builder.addNewWire<32>(
        "SLL_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("SLL_JOIN", "OUT"),
        {getOutputPin<32>("SLL_OUT")});
    builder.addNewWire<32>(
        "SRL_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("SRL_JOIN", "OUT"),
        {getOutputPin<32>("SRL_OUT")});
    builder.addNewWire<32>(
        "SRA_bus_internal",
        builder.getOutputPin<BitJoiner<32>, 32>("SRA_JOIN", "OUT"),
        {getOutputPin<32>("SRA_OUT")});
}
