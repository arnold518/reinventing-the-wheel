#include "modules/basic/Decoder.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Constant.hpp"
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
void buildDecoder(IOComponent& component, ComponentBuilder& builder,bool zero_output_unused=false) {
    static_assert(ADDR_WIDTH >= 2, "decoder helper expects at least two address bits");

    constexpr size_t OutputCount = decoderOutputCount<ADDR_WIDTH>();
    builder.addNewComponent<BitSplitter<ADDR_WIDTH>>("ADDR_SPLIT");

    if constexpr (ADDR_WIDTH == 5) {
        for (size_t bit = 0; bit < 5; ++bit)
            builder.addNewComponent<NOTGate>(notGateName(bit));
        for (size_t low = 0; low < 4; ++low)
            builder.addNewComponent<ANDGate>("LOW_" + std::to_string(low));
        for (size_t high = 0; high < 8; ++high) {
            builder.addNewComponent<ANDGate>("HIGH_PAIR_" + std::to_string(high));
            builder.addNewComponent<ANDGate>("HIGH_" + std::to_string(high));
        }
        for (size_t output = 0; output < OutputCount; ++output) {
            if (output==0 && zero_output_unused) continue;
            builder.addNewComponent<ANDGate>("MATCH_" + std::to_string(output));
            builder.addNewComponent<ANDGate>(enableGateName(output));
        }
        if (zero_output_unused) {
            auto zero=builder.addNewComponent<ConstantValue<1>>("OUT0_ZERO",0);
            builder.addNewWire("OUT0_constant",zero->getOutputPin("OUT"),{component.getOutputPin("OUT0")});
        }
        builder.addNewWire<5>("ADDR_internal", component.getInputPin<5>("ADDR"),
            {builder.getInputPin<BitSplitter<5>, 5>("ADDR_SPLIT", "IN")});
        std::array<std::vector<std::shared_ptr<Pin<>>>, 5> positive_sinks, negative_sinks;
        for (size_t bit = 0; bit < 5; ++bit)
            positive_sinks[bit].push_back(builder.getInputPin<NOTGate>(notGateName(bit), "IN"));
        auto literal = [&](size_t bit, bool positive, std::shared_ptr<Pin<>> sink) {
            (positive ? positive_sinks[bit] : negative_sinks[bit]).push_back(std::move(sink));
        };
        for (size_t low = 0; low < 4; ++low) {
            const auto gate = "LOW_" + std::to_string(low);
            literal(0, low & 1U, builder.getInputPin<ANDGate>(gate, "A"));
            literal(1, low & 2U, builder.getInputPin<ANDGate>(gate, "B"));
        }
        for (size_t high = 0; high < 8; ++high) {
            const auto suffix = std::to_string(high);
            literal(2, high & 1U, builder.getInputPin<ANDGate>("HIGH_PAIR_" + suffix, "A"));
            literal(3, high & 2U, builder.getInputPin<ANDGate>("HIGH_PAIR_" + suffix, "B"));
            builder.addNewWire("HIGH_PAIR_" + suffix + "_to_HIGH",
                builder.getOutputPin<ANDGate>("HIGH_PAIR_" + suffix, "OUT"),
                {builder.getInputPin<ANDGate>("HIGH_" + suffix, "A")});
            literal(4, high & 4U, builder.getInputPin<ANDGate>("HIGH_" + suffix, "B"));
        }
        for (size_t bit = 0; bit < 5; ++bit) {
            builder.addNewWire("ADDR" + std::to_string(bit) + "_internal",
                builder.getOutputPin<BitSplitter<5>>("ADDR_SPLIT", "OUT_" + std::to_string(bit)),
                positive_sinks[bit]);
            builder.addNewWire("NOT_ADDR" + std::to_string(bit) + "_internal",
                builder.getOutputPin<NOTGate>(notGateName(bit), "OUT"), negative_sinks[bit]);
        }
        std::vector<std::shared_ptr<Pin<>>> enable_sinks;
        std::array<std::vector<std::shared_ptr<Pin<>>>, 4> low_sinks;
        std::array<std::vector<std::shared_ptr<Pin<>>>, 8> high_sinks;
        for (size_t output = 0; output < OutputCount; ++output) {
            if (output==0 && zero_output_unused) continue;
            const auto suffix = std::to_string(output);
            low_sinks[output & 3U].push_back(builder.getInputPin<ANDGate>("MATCH_" + suffix, "A"));
            high_sinks[output >> 2U].push_back(builder.getInputPin<ANDGate>("MATCH_" + suffix, "B"));
            builder.addNewWire("MATCH_TO_ENABLE_" + suffix,
                builder.getOutputPin<ANDGate>("MATCH_" + suffix, "OUT"),
                {builder.getInputPin<ANDGate>(enableGateName(output), "B")});
            enable_sinks.push_back(builder.getInputPin<ANDGate>(enableGateName(output), "A"));
            builder.addNewWire("OUT" + suffix + "_internal",
                builder.getOutputPin<ANDGate>(enableGateName(output), "OUT"),
                {component.getOutputPin(outputPinName(output))});
        }
        for (size_t low = 0; low < low_sinks.size(); ++low)
            builder.addNewWire("LOW_" + std::to_string(low) + "_fanout",
                builder.getOutputPin<ANDGate>("LOW_" + std::to_string(low), "OUT"), low_sinks[low]);
        for (size_t high = 0; high < high_sinks.size(); ++high)
            builder.addNewWire("HIGH_" + std::to_string(high) + "_fanout",
                builder.getOutputPin<ANDGate>("HIGH_" + std::to_string(high), "OUT"), high_sinks[high]);
        builder.addNewWire("ENABLE_internal", component.getInputPin("ENABLE"), enable_sinks);
        return;
    }

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

Decoder5to32::Decoder5to32(std::string name,bool zero_output_unused)
    : IOComponent(std::move(name), [](IOComponent* self) {
        addDecoderPins<5>(self);
    }),zero_output_unused_(zero_output_unused) {}

void Decoder5to32::buildInternals(ComponentBuilder& builder) {
    buildDecoder<5>(*this, builder,zero_output_unused_);
}
