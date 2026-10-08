#include "components/StructuralNetBuilder.hpp"
#include "modules/basic/Mux.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "basic/LogicValueHelpers.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.hpp"
#include "components/selection/FamilyFactories.hpp"
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
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<NOTGate>("NOT_SEL");
    builder.addNewComponent<ANDGate>("AND_A");
    builder.addNewComponent<ANDGate>("AND_B");
    builder.addNewComponent<ORGate>("OR_OUT");

    w.connect(component.getInputPin("SEL"),
        {builder.getInputPin<NOTGate>("NOT_SEL", "IN"), builder.getInputPin<ANDGate>("AND_B", "B")});
    w.connect(component.getInputPin("A"), {builder.getInputPin<ANDGate>("AND_A", "A")});
    w.connect(component.getInputPin("B"), {builder.getInputPin<ANDGate>("AND_B", "A")});
    w.connect(builder.getOutputPin<NOTGate>("NOT_SEL", "OUT"), {builder.getInputPin<ANDGate>("AND_A", "B")});
    w.connect(builder.getOutputPin<ANDGate>("AND_A", "OUT"), {builder.getInputPin<ORGate>("OR_OUT", "A")});
    w.connect(builder.getOutputPin<ANDGate>("AND_B", "OUT"), {builder.getInputPin<ORGate>("OR_OUT", "B")});
    w.connect(builder.getOutputPin<ORGate>("OR_OUT", "OUT"), {component.getOutputPin("OUT")});
    w.finish();
}

template<size_t SEL_WIDTH>
void buildOneBitMuxTree(IOComponent& component, ComponentBuilder& builder, size_t input_count) {
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<SEL_WIDTH>>("SEL_SPLIT");
    w.connect(component.getInputPin<SEL_WIDTH>("SEL"),
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
                w.connect(component.getInputPin(muxInputName(input_count, 2 * i)),
                    {builder.getInputPin<Mux2to1>(mux_name, "A")});
                w.connect(component.getInputPin(muxInputName(input_count, 2 * i + 1)),
                    {builder.getInputPin<Mux2to1>(mux_name, "B")});
            } else {
                w.connect(builder.getOutputPin<Mux2to1>(previous_level[2 * i], "OUT"),
                    {builder.getInputPin<Mux2to1>(mux_name, "A")});
                w.connect(builder.getOutputPin<Mux2to1>(previous_level[2 * i + 1], "OUT"),
                    {builder.getInputPin<Mux2to1>(mux_name, "B")});
            }
        }

        w.connect(builder.getOutputPin<BitSplitter<SEL_WIDTH>>("SEL_SPLIT", "OUT_" + std::to_string(level)),
            select_sinks);

        previous_level = std::move(current_level);
        level_input_count = mux_count;
        ++level;
    }

    w.connect(builder.getOutputPin<Mux2to1>(previous_level.front(), "OUT"),
        {component.getOutputPin("OUT")});
    w.finish();
}

template<typename OneBitMuxT, size_t INPUT_COUNT, size_t SEL_WIDTH, size_t BUS_WIDTH>
void buildBusMux(IOComponent& component, ComponentBuilder& builder) {
    circuit::detail::StructuralWiring w(builder);
    for (size_t input_index = 0; input_index < INPUT_COUNT; ++input_index) {
        const auto input_name = muxInputName(INPUT_COUNT, input_index);
        const auto splitter_name = input_name + "_SPLIT";
        builder.addNewComponent<BitSplitter<BUS_WIDTH>>(splitter_name);
        w.connect(component.getInputPin<BUS_WIDTH>(input_name),
            {builder.getInputPin<BitSplitter<BUS_WIDTH>, BUS_WIDTH>(splitter_name, "IN")});
    }

    builder.addNewComponent<BitJoiner<BUS_WIDTH>>("OUT_JOIN");

    if constexpr (INPUT_COUNT == 2 && SEL_WIDTH == 1) {
        builder.addNewComponent<NOTGate>("NOT_SEL");
        std::vector<std::shared_ptr<Pin<>>> selected_sinks{
            builder.getInputPin<NOTGate>("NOT_SEL", "IN")};
        std::vector<std::shared_ptr<Pin<>>> unselected_sinks;
        for (size_t bit = 0; bit < BUS_WIDTH; ++bit) {
            const auto suffix = std::to_string(bit);
            const auto a = "SELECT_A_" + suffix;
            const auto b = "SELECT_B_" + suffix;
            const auto out = "COMBINE_" + suffix;
            builder.addNewComponent<ANDGate>(a);
            builder.addNewComponent<ANDGate>(b);
            builder.addNewComponent<ORGate>(out);
            w.connect(builder.getOutputPin<BitSplitter<BUS_WIDTH>>("A_SPLIT", "OUT_" + suffix),
                {builder.getInputPin<ANDGate>(a, "A")});
            w.connect(builder.getOutputPin<BitSplitter<BUS_WIDTH>>("B_SPLIT", "OUT_" + suffix),
                {builder.getInputPin<ANDGate>(b, "A")});
            selected_sinks.push_back(builder.getInputPin<ANDGate>(b, "B"));
            unselected_sinks.push_back(builder.getInputPin<ANDGate>(a, "B"));
            w.connect(builder.getOutputPin<ANDGate>(a, "OUT"),
                {builder.getInputPin<ORGate>(out, "A")});
            w.connect(builder.getOutputPin<ANDGate>(b, "OUT"),
                {builder.getInputPin<ORGate>(out, "B")});
            w.connect(builder.getOutputPin<ORGate>(out, "OUT"),
                {builder.getInputPin<BitJoiner<BUS_WIDTH>>("OUT_JOIN", "IN_" + suffix)});
        }
        w.connect(component.getInputPin("SEL"), selected_sinks);
        w.connect(builder.getOutputPin<NOTGate>("NOT_SEL", "OUT"),
            unselected_sinks);
        w.connect(builder.getOutputPin<BitJoiner<BUS_WIDTH>, BUS_WIDTH>("OUT_JOIN", "OUT"),
            {component.getOutputPin<BUS_WIDTH>("OUT")});
        w.finish();
        return;
    }

    std::vector<std::shared_ptr<Pin<SEL_WIDTH>>> select_sinks;
    select_sinks.reserve(BUS_WIDTH);

    for (size_t bit = 0; bit < BUS_WIDTH; ++bit) {
        const auto bit_text = std::to_string(bit);
        const auto mux_name = "MUX_BIT_" + bit_text;
        builder.addNewComponent<OneBitMuxT>(mux_name);
        select_sinks.push_back(builder.getInputPin<OneBitMuxT, SEL_WIDTH>(mux_name, "SEL"));

        for (size_t input_index = 0; input_index < INPUT_COUNT; ++input_index) {
            const auto input_name = muxInputName(INPUT_COUNT, input_index);
            const auto splitter_name = input_name + "_SPLIT";
            w.connect(builder.getOutputPin<BitSplitter<BUS_WIDTH>>(splitter_name, "OUT_" + bit_text),
                {builder.getInputPin<OneBitMuxT>(mux_name, muxInputName(INPUT_COUNT, input_index))});
        }

        w.connect(builder.getOutputPin<OneBitMuxT>(mux_name, "OUT"),
            {builder.getInputPin<BitJoiner<BUS_WIDTH>>("OUT_JOIN", "IN_" + bit_text)});
    }

    w.connect(component.getInputPin<SEL_WIDTH>("SEL"),
        select_sinks);

    w.connect(builder.getOutputPin<BitJoiner<BUS_WIDTH>, BUS_WIDTH>("OUT_JOIN", "OUT"),
        {component.getOutputPin<BUS_WIDTH>("OUT")});
    w.finish();
}

template<size_t INPUT_COUNT, size_t SEL_WIDTH, size_t BUS_WIDTH>
void defineBusMuxPins(IOComponent* self) {
    for (size_t input = 0; input < INPUT_COUNT; ++input)
        self->addPin<BUS_WIDTH>(muxInputName(INPUT_COUNT, input), PinType::INPUT);
    self->addPin<SEL_WIDTH>("SEL", PinType::INPUT);
    self->addPin<BUS_WIDTH>("OUT", PinType::OUTPUT);
}

// Each output bit is the same tree of Mux2to1 the structural form builds:
// level 0 pairs neighbouring inputs on SEL bit 0, level 1 on SEL bit 1, ...
template<size_t INPUT_COUNT, size_t SEL_WIDTH, size_t BUS_WIDTH>
class BusMuxBehavioral final : public BasicComponent {
public:
    BusMuxBehavioral(std::string name, const char* type_name)
        : BasicComponent(std::move(name), 1, defineBusMuxPins<INPUT_COUNT, SEL_WIDTH, BUS_WIDTH>),
          type_name_(type_name) {}
    const char* getTypeName() const override { return type_name_; }
    void evaluate(size_t current_time, Simulator& simulator) override {
        const auto select = getInputPin<SEL_WIDTH>("SEL")->getValueAsVector();
        std::vector<std::vector<LogicValue>> inputs;
        inputs.reserve(INPUT_COUNT);
        for (size_t input = 0; input < INPUT_COUNT; ++input)
            inputs.push_back(getInputPin<BUS_WIDTH>(muxInputName(INPUT_COUNT, input))->getValueAsVector());
        std::vector<LogicValue> out(BUS_WIDTH);
        for (size_t bit = 0; bit < BUS_WIDTH; ++bit) {
            std::vector<LogicValue> level(INPUT_COUNT);
            for (size_t input = 0; input < INPUT_COUNT; ++input) level[input] = inputs[input][bit];
            for (size_t stage = 0; level.size() > 1; ++stage) {
                std::vector<LogicValue> next(level.size() / 2);
                for (size_t pair = 0; pair < next.size(); ++pair)
                    next[pair] = circuit::logic_values::mux2(level[2 * pair], level[2 * pair + 1], select[stage]);
                level = std::move(next);
            }
            out[bit] = level.front();
        }
        _updateOutputWire<BUS_WIDTH>(simulator, "OUT", out, current_time);
    }
private:
    const char* type_name_;
};
}

namespace circuit::families {
namespace {
template<class Structural, size_t INPUT_COUNT, size_t SEL_WIDTH>
ComponentFamily busMuxFamily(std::string_view contract_id) {
    return ComponentFamily{
        contract_id, Structural::TypeName, defineBusMuxPins<INPUT_COUNT, SEL_WIDTH, 32>,
        structuralFactory<Structural>(),
        [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
            return Component::createWithContext<BusMuxBehavioral<INPUT_COUNT, SEL_WIDTH, 32>>(
                context, name, Structural::TypeName);
        }};
}
}
const ComponentFamily Mux2to1_32bit = busMuxFamily<::Mux2to1_32bit, 2, 1>("mux.2x1.width32");
const ComponentFamily Mux4to1_32bit = busMuxFamily<::Mux4to1_32bit, 4, 2>("mux.4x1.width32");
const ComponentFamily Mux8to1_32bit = busMuxFamily<::Mux8to1_32bit, 8, 3>("mux.8x1.width32");
const ComponentFamily Mux32to1_32bit = busMuxFamily<::Mux32to1_32bit, 32, 5>("mux.32x1.width32");
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

Mux32to1::Mux32to1(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        for (int i = 0; i < 32; ++i) self->addPin("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<5>("SEL", PinType::INPUT);
        self->addPin("OUT", PinType::OUTPUT);
    }) {}

void Mux32to1::buildInternals(ComponentBuilder& builder) {
    buildOneBitMuxTree<5>(*this, builder, 32);
}

Mux2to1_4bit::Mux2to1_4bit(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        self->addPin<4>("A", PinType::INPUT);
        self->addPin<4>("B", PinType::INPUT);
        self->addPin("SEL", PinType::INPUT);
        self->addPin<4>("OUT", PinType::OUTPUT);
    }) {}

void Mux2to1_4bit::buildInternals(ComponentBuilder& builder) {
    buildBusMux<Mux2to1, 2, 1, 4>(*this, builder);
}

Mux2to1_32bit::Mux2to1_32bit(std::string name)
    : IOComponent(std::move(name), defineBusMuxPins<2, 1, 32>) {}

void Mux2to1_32bit::buildInternals(ComponentBuilder& builder) {
    buildBusMux<Mux2to1, 2, 1, 32>(*this, builder);
}

Mux4to1_32bit::Mux4to1_32bit(std::string name)
    : IOComponent(std::move(name), defineBusMuxPins<4, 2, 32>) {}

void Mux4to1_32bit::buildInternals(ComponentBuilder& builder) {
    buildBusMux<Mux4to1, 4, 2, 32>(*this, builder);
}

Mux8to1_8bit::Mux8to1_8bit(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        for (int i = 0; i < 8; ++i) self->addPin<8>("IN" + std::to_string(i), PinType::INPUT);
        self->addPin<3>("SEL", PinType::INPUT);
        self->addPin<8>("OUT", PinType::OUTPUT);
    }) {}

void Mux8to1_8bit::buildInternals(ComponentBuilder& builder) {
    buildBusMux<Mux8to1, 8, 3, 8>(*this, builder);
}

Mux8to1_32bit::Mux8to1_32bit(std::string name)
    : IOComponent(std::move(name), defineBusMuxPins<8, 3, 32>) {}

void Mux8to1_32bit::buildInternals(ComponentBuilder& builder) {
    buildBusMux<Mux8to1, 8, 3, 32>(*this, builder);
}

Mux32to1_32bit::Mux32to1_32bit(std::string name)
    : IOComponent(std::move(name), defineBusMuxPins<32, 5, 32>) {}

void Mux32to1_32bit::buildInternals(ComponentBuilder& builder) {
    buildBusMux<Mux32to1, 32, 5, 32>(*this, builder);
}
