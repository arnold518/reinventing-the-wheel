#include "modules/rv32i/RV32IBitPatternMatcher.hpp"
#include "components/StructuralNetBuilder.hpp"

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
    circuit::detail::StructuralWiring w(builder);
    builder.addNewComponent<BitSplitter<32>>("INPUT_SPLIT");
    w.connect(getInputPin<32>("INPUT"),
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
        w.connect(source,
            {builder.getInputPin<NOTGate>(not_name, "IN")});
        terms.push_back(builder.getOutputPin<NOTGate>(not_name, "OUT"));
    }

    if (terms.empty()) {
        throw std::invalid_argument("RV32IBitPatternMatcher requires a nonzero mask");
    }
    if (terms.size() == 1) {
        w.connect(terms.front(), {getOutputPin("MATCH")});
        w.finish();
        return;
    }

    std::vector<circuit::detail::StructuralWiring::PinPtr> inputs(terms.begin(),terms.end());
    w.connect(w.reduce("MATCH_TREE",inputs,true),getOutputPin("MATCH"));
    w.finish();
}
