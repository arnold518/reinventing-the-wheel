#include "modules/basic/Mux.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <string>
#include <vector>

namespace {
std::string muxInputName(size_t input_count, size_t index) {
    if (input_count == 2) {
        return index == 0 ? "A" : "B";
    }
    return "IN" + std::to_string(index);
}

void buildMux2FromGates(IOComponent& component, ComponentBuilder& builder) {
    builder.addNewComponent<NOTGate>("NOT_SEL");
    builder.addNewComponent<ANDGate>("AND_A");
    builder.addNewComponent<ANDGate>("AND_B");
    builder.addNewComponent<ORGate>("OR_OUT");

    builder.addNewWire(
        "SEL_internal",
        component.getInputPin("SEL"),
        {builder.getInputPin<NOTGate>("NOT_SEL", "IN"), builder.getInputPin<ANDGate>("AND_B", "B")});
    builder.addNewWire("A_to_AND", component.getInputPin("A"), {builder.getInputPin<ANDGate>("AND_A", "A")});
    builder.addNewWire("B_to_AND", component.getInputPin("B"), {builder.getInputPin<ANDGate>("AND_B", "A")});
    builder.addNewWire("NOT_SEL_to_AND", builder.getOutputPin<NOTGate>("NOT_SEL", "OUT"), {builder.getInputPin<ANDGate>("AND_A", "B")});
    builder.addNewWire("AND_A_to_OR", builder.getOutputPin<ANDGate>("AND_A", "OUT"), {builder.getInputPin<ORGate>("OR_OUT", "A")});
    builder.addNewWire("AND_B_to_OR", builder.getOutputPin<ANDGate>("AND_B", "OUT"), {builder.getInputPin<ORGate>("OR_OUT", "B")});
    builder.addNewWire("OR_to_OUT", builder.getOutputPin<ORGate>("OR_OUT", "OUT"), {component.getOutputPin("OUT")});
}

template<size_t SEL_WIDTH>
void buildOneBitMuxTree(IOComponent& component, ComponentBuilder& builder, size_t input_count) {
    builder.addNewComponent<BitSplitter<SEL_WIDTH>>("SEL_SPLIT");
    builder.addNewWire<SEL_WIDTH>(
        "SEL_bus_internal",
        component.getInputPin<SEL_WIDTH>("SEL"),
        {builder.getInputPin<BitSplitter<SEL_WIDTH>, SEL_WIDTH>("SEL_SPLIT", "IN")});

    std::vector<std::string> previous_level;
    size_t level_input_count = input_count;
    size_t level = 0;

    while (level_input_count > 1) {
        std::vector<std::string> current_level;
        std::vector<std::shared_ptr<Pin<>>> select_sinks;
        const size_t mux_count = level_input_count / 2;

        for (size_t i = 0; i < mux_count; ++i) {
            const auto mux_name = "MUX_L" + std::to_string(level) + "_" + std::to_string(i);
            builder.addNewComponent<Mux2to1>(mux_name);
            current_level.push_back(mux_name);
            select_sinks.push_back(builder.getInputPin<Mux2to1>(mux_name, "SEL"));

            if (level == 0) {
                builder.addNewWire(
                    "IN" + std::to_string(2 * i) + "_to_" + mux_name,
                    component.getInputPin(muxInputName(input_count, 2 * i)),
                    {builder.getInputPin<Mux2to1>(mux_name, "A")});
                builder.addNewWire(
                    "IN" + std::to_string(2 * i + 1) + "_to_" + mux_name,
                    component.getInputPin(muxInputName(input_count, 2 * i + 1)),
                    {builder.getInputPin<Mux2to1>(mux_name, "B")});
            } else {
                builder.addNewWire(
                    previous_level[2 * i] + "_to_" + mux_name,
                    builder.getOutputPin<Mux2to1>(previous_level[2 * i], "OUT"),
                    {builder.getInputPin<Mux2to1>(mux_name, "A")});
                builder.addNewWire(
                    previous_level[2 * i + 1] + "_to_" + mux_name,
                    builder.getOutputPin<Mux2to1>(previous_level[2 * i + 1], "OUT"),
                    {builder.getInputPin<Mux2to1>(mux_name, "B")});
            }
        }

        builder.addNewWire(
            "SEL_bit_" + std::to_string(level),
            builder.getOutputPin<BitSplitter<SEL_WIDTH>>("SEL_SPLIT", "OUT_" + std::to_string(level)),
            select_sinks);

        previous_level = std::move(current_level);
        level_input_count = mux_count;
        ++level;
    }

    builder.addNewWire(
        "MUX_tree_to_OUT",
        builder.getOutputPin<Mux2to1>(previous_level.front(), "OUT"),
        {component.getOutputPin("OUT")});
}

template<typename OneBitMuxT, size_t INPUT_COUNT, size_t SEL_WIDTH>
void build8BitMux(IOComponent& component, ComponentBuilder& builder) {
    for (size_t input_index = 0; input_index < INPUT_COUNT; ++input_index) {
        const auto input_name = muxInputName(INPUT_COUNT, input_index);
        const auto splitter_name = input_name + "_SPLIT";
        builder.addNewComponent<BitSplitter<8>>(splitter_name);
        builder.addNewWire<8>(
            input_name + "_bus_internal",
            component.getInputPin<8>(input_name),
            {builder.getInputPin<BitSplitter<8>, 8>(splitter_name, "IN")});
    }

    builder.addNewComponent<BitJoiner<8>>("OUT_JOIN");

    std::vector<std::shared_ptr<Pin<SEL_WIDTH>>> select_sinks;
    select_sinks.reserve(8);

    for (size_t bit = 0; bit < 8; ++bit) {
        const auto bit_text = std::to_string(bit);
        const auto mux_name = "MUX_BIT_" + bit_text;
        builder.addNewComponent<OneBitMuxT>(mux_name);
        select_sinks.push_back(builder.getInputPin<OneBitMuxT, SEL_WIDTH>(mux_name, "SEL"));

        for (size_t input_index = 0; input_index < INPUT_COUNT; ++input_index) {
            const auto input_name = muxInputName(INPUT_COUNT, input_index);
            const auto splitter_name = input_name + "_SPLIT";
            builder.addNewWire(
                input_name + "_bit_" + bit_text + "_to_" + mux_name,
                builder.getOutputPin<BitSplitter<8>>(splitter_name, "OUT_" + bit_text),
                {builder.getInputPin<OneBitMuxT>(mux_name, muxInputName(INPUT_COUNT, input_index))});
        }

        builder.addNewWire(
            mux_name + "_to_JOIN_" + bit_text,
            builder.getOutputPin<OneBitMuxT>(mux_name, "OUT"),
            {builder.getInputPin<BitJoiner<8>>("OUT_JOIN", "IN_" + bit_text)});
    }

    builder.addNewWire<SEL_WIDTH>(
        "SEL_bus_internal",
        component.getInputPin<SEL_WIDTH>("SEL"),
        select_sinks);

    builder.addNewWire<8>(
        "OUT_bus_internal",
        builder.getOutputPin<BitJoiner<8>, 8>("OUT_JOIN", "OUT"),
        {component.getOutputPin<8>("OUT")});
}
}

Mux2to1::Mux2to1(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin("A", PinType::INPUT);
        self->addPin("B", PinType::INPUT);
        self->addPin("SEL", PinType::INPUT);
        self->addPin("OUT", PinType::OUTPUT);
    }) {}

void Mux2to1::buildInternals(ComponentBuilder& builder) {
    buildMux2FromGates(*this, builder);
}

Mux4to1::Mux4to1(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        for (int i = 0; i < 4; ++i) self->addPin("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<2>("SEL", PinType::INPUT);
        self->addPin("OUT", PinType::OUTPUT);
    }) {}

void Mux4to1::buildInternals(ComponentBuilder& builder) {
    buildOneBitMuxTree<2>(*this, builder, 4);
}

Mux8to1::Mux8to1(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        for (int i = 0; i < 8; ++i) self->addPin("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<3>("SEL", PinType::INPUT);
        self->addPin("OUT", PinType::OUTPUT);
    }) {}

void Mux8to1::buildInternals(ComponentBuilder& builder) {
    buildOneBitMuxTree<3>(*this, builder, 8);
}

Mux16to1::Mux16to1(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        for (int i = 0; i < 16; ++i) self->addPin("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<4>("SEL", PinType::INPUT);
        self->addPin("OUT", PinType::OUTPUT);
    }) {}

void Mux16to1::buildInternals(ComponentBuilder& builder) {
    buildOneBitMuxTree<4>(*this, builder, 16);
}

Mux2to1_8bit::Mux2to1_8bit(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<8>("A", PinType::INPUT);
        self->addPin<8>("B", PinType::INPUT);
        self->addPin("SEL", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void Mux2to1_8bit::buildInternals(ComponentBuilder& builder) {
    build8BitMux<Mux2to1, 2, 1>(*this, builder);
}

Mux4to1_8bit::Mux4to1_8bit(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        for (int i = 0; i < 4; ++i) self->addPin<8>("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<2>("SEL", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void Mux4to1_8bit::buildInternals(ComponentBuilder& builder) {
    build8BitMux<Mux4to1, 4, 2>(*this, builder);
}

Mux8to1_8bit::Mux8to1_8bit(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        for (int i = 0; i < 8; ++i) self->addPin<8>("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<3>("SEL", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void Mux8to1_8bit::buildInternals(ComponentBuilder& builder) {
    build8BitMux<Mux8to1, 8, 3>(*this, builder);
}

Mux16to1_8bit::Mux16to1_8bit(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        for (int i = 0; i < 16; ++i) self->addPin<8>("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<4>("SEL", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void Mux16to1_8bit::buildInternals(ComponentBuilder& builder) {
    build8BitMux<Mux16to1, 16, 4>(*this, builder);
}
