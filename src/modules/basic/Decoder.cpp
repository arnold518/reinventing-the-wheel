#include "modules/basic/Decoder.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace {
template<size_t ADDR_WIDTH>
constexpr size_t decoderOutputCount() {
    return size_t{1} << ADDR_WIDTH;
}

std::string outputPinName(size_t index) {
    return "OUT" + std::to_string(index);
}

std::string notGateName(size_t bit) {
    return "NOT_ADDR" + std::to_string(bit);
}

std::string matchGateName(size_t output, size_t stage) {
    return outputPinName(output) + "_MATCH_AND_" + std::to_string(stage);
}

std::string enableGateName(size_t output) {
    return outputPinName(output) + "_ENABLE_AND";
}

template<size_t ADDR_WIDTH>
void addDecoderPins(IOComponent* self) {
    self->addPin<ADDR_WIDTH>("ADDR", PinType::INPUT);
    self->addPin("ENABLE", PinType::INPUT);
    for (size_t output = 0; output < decoderOutputCount<ADDR_WIDTH>(); ++output) {
        self->addPin(outputPinName(output), PinType::OUTPUT);
    }
}

template<size_t ADDR_WIDTH>
void buildDecoder(IOComponent& component, ComponentBuilder& builder) {
    static_assert(ADDR_WIDTH >= 2, "decoder helper expects at least two address bits");

    constexpr size_t OutputCount = decoderOutputCount<ADDR_WIDTH>();
    builder.addNewComponent<BitSplitter<ADDR_WIDTH>>("ADDR_SPLIT");

    for (size_t bit = 0; bit < ADDR_WIDTH; ++bit) {
        builder.addNewComponent<NOTGate>(notGateName(bit));
    }

    for (size_t output = 0; output < OutputCount; ++output) {
        for (size_t stage = 0; stage < ADDR_WIDTH - 1; ++stage) {
            builder.addNewComponent<ANDGate>(matchGateName(output, stage));
        }
        builder.addNewComponent<ANDGate>(enableGateName(output));
    }

    builder.addNewWire<ADDR_WIDTH>(
        "ADDR_internal",
        component.getInputPin<ADDR_WIDTH>("ADDR"),
        {builder.getInputPin<BitSplitter<ADDR_WIDTH>, ADDR_WIDTH>("ADDR_SPLIT", "IN")});

    std::array<std::vector<std::shared_ptr<Pin<>>>, ADDR_WIDTH> high_sinks;
    std::array<std::vector<std::shared_ptr<Pin<>>>, ADDR_WIDTH> low_sinks;
    std::vector<std::shared_ptr<Pin<>>> enable_sinks;
    enable_sinks.reserve(OutputCount);

    for (size_t output = 0; output < OutputCount; ++output) {
        for (size_t bit = 0; bit < ADDR_WIDTH; ++bit) {
            std::shared_ptr<Pin<>> sink;
            if (bit == 0) {
                sink = builder.getInputPin<ANDGate>(matchGateName(output, 0), "A");
            } else if (bit == 1) {
                sink = builder.getInputPin<ANDGate>(matchGateName(output, 0), "B");
            } else {
                sink = builder.getInputPin<ANDGate>(matchGateName(output, bit - 1), "B");
            }

            if ((output >> bit) & 1U) {
                high_sinks[bit].push_back(sink);
            } else {
                low_sinks[bit].push_back(sink);
            }
        }

        for (size_t stage = 1; stage < ADDR_WIDTH - 1; ++stage) {
            builder.addNewWire(
                outputPinName(output) + "_MATCH_STAGE_" + std::to_string(stage),
                builder.getOutputPin<ANDGate>(matchGateName(output, stage - 1), "OUT"),
                {builder.getInputPin<ANDGate>(matchGateName(output, stage), "A")});
        }

        builder.addNewWire(
            outputPinName(output) + "_MATCH_internal",
            builder.getOutputPin<ANDGate>(matchGateName(output, ADDR_WIDTH - 2), "OUT"),
            {builder.getInputPin<ANDGate>(enableGateName(output), "B")});

        enable_sinks.push_back(builder.getInputPin<ANDGate>(enableGateName(output), "A"));

        builder.addNewWire(
            outputPinName(output) + "_internal",
            builder.getOutputPin<ANDGate>(enableGateName(output), "OUT"),
            {component.getOutputPin(outputPinName(output))});
    }

    for (size_t bit = 0; bit < ADDR_WIDTH; ++bit) {
        const auto bit_text = std::to_string(bit);
        high_sinks[bit].push_back(builder.getInputPin<NOTGate>(notGateName(bit), "IN"));

        builder.addNewWire(
            "ADDR" + bit_text + "_internal",
            builder.getOutputPin<BitSplitter<ADDR_WIDTH>>("ADDR_SPLIT", "OUT_" + bit_text),
            high_sinks[bit]);

        builder.addNewWire(
            "NOT_ADDR" + bit_text + "_internal",
            builder.getOutputPin<NOTGate>(notGateName(bit), "OUT"),
            low_sinks[bit]);
    }

    builder.addNewWire("ENABLE_internal", component.getInputPin("ENABLE"), enable_sinks);
}
}

Decoder2to4::Decoder2to4(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        addDecoderPins<2>(self);
    }) {}

void Decoder2to4::buildInternals(ComponentBuilder& builder) {
    buildDecoder<2>(*this, builder);
}

Decoder5to32::Decoder5to32(std::string name)
    : IOComponent(std::move(name), [](IOComponent* self) {
        addDecoderPins<5>(self);
    }) {}

void Decoder5to32::buildInternals(ComponentBuilder& builder) {
    buildDecoder<5>(*this, builder);
}
