#include "modules/rv32i/RV32IBitPatternMatcher.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/BitAdapter.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

RV32IBitPatternMatcher::RV32IBitPatternMatcher(std::string name, uint32_t mask, uint32_t value)
    : IOComponent(std::move(name), [](IOComponent* self) {
          self->addPin<32>("INPUT", PinType::INPUT);
          self->addPin("MATCH", PinType::OUTPUT);
      }),
      mask_(mask),
      value_(value & mask) {}

void RV32IBitPatternMatcher::buildInternals(ComponentBuilder& builder) {
    builder.addNewComponent<BitSplitter<32>>("INPUT_SPLIT");
    builder.addNewWire<32>(
        "INPUT_bus_internal",
        getInputPin<32>("INPUT"),
        {builder.getInputPin<BitSplitter<32>, 32>("INPUT_SPLIT", "IN")});

    std::vector<std::shared_ptr<Pin<>>> terms;
    for (size_t bit = 0; bit < 32; ++bit) {
        if ((mask_ & (uint32_t{1} << bit)) == 0) {
            continue;
        }

        const auto bit_text = std::to_string(bit);
        auto source = builder.getOutputPin<BitSplitter<32>>("INPUT_SPLIT", "OUT_" + bit_text);
        if ((value_ & (uint32_t{1} << bit)) != 0) {
            terms.push_back(source);
            continue;
        }

        const auto not_name = "NOT_BIT_" + bit_text;
        builder.addNewComponent<NOTGate>(not_name);
        builder.addNewWire(
            "BIT_" + bit_text + "_to_NOT",
            source,
            {builder.getInputPin<NOTGate>(not_name, "IN")});
        terms.push_back(builder.getOutputPin<NOTGate>(not_name, "OUT"));
    }

    if (terms.empty()) {
        throw std::invalid_argument("RV32IBitPatternMatcher requires a nonzero mask");
    }
    if (terms.size() == 1) {
        builder.addNewWire("single_term_to_MATCH", terms.front(), {getOutputPin("MATCH")});
        return;
    }

    builder.addNewComponent<ANDGate>("AND_1");
    builder.addNewWire("TERM_0_to_AND_1", terms[0], {builder.getInputPin<ANDGate>("AND_1", "A")});
    builder.addNewWire("TERM_1_to_AND_1", terms[1], {builder.getInputPin<ANDGate>("AND_1", "B")});

    std::string previous = "AND_1";
    for (size_t term = 2; term < terms.size(); ++term) {
        const auto gate = "AND_" + std::to_string(term);
        builder.addNewComponent<ANDGate>(gate);
        builder.addNewWire(
            previous + "_to_" + gate,
            builder.getOutputPin<ANDGate>(previous, "OUT"),
            {builder.getInputPin<ANDGate>(gate, "A")});
        builder.addNewWire(
            "TERM_" + std::to_string(term) + "_to_" + gate,
            terms[term],
            {builder.getInputPin<ANDGate>(gate, "B")});
        previous = gate;
    }

    builder.addNewWire(
        "match_chain_to_MATCH",
        builder.getOutputPin<ANDGate>(previous, "OUT"),
        {getOutputPin("MATCH")});
}
