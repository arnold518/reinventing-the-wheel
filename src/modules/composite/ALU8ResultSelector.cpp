#include "modules/composite/ALU8ResultSelector.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"

ALU8ResultSelector::ALU8ResultSelector(std::string name)
    : IOComponent(std::move(name), circuit::families::ALU8ResultSelector.pinInitializer()) {}

void ALU8ResultSelector::buildInternals(ComponentBuilder& builder) {
    using Net = circuit::detail::Structural8Builder;
    Net n(*this, builder);
    std::array<Net::Signal, 6> enables{n.input("ARITHMETIC_ENABLE"), n.input("LOGIC_ENABLE"), n.input("SHIFT_ENABLE"), n.input("COMPARE_ENABLE"), n.input("PASS_A_ENABLE"), n.input("PASS_B_ENABLE")};
    std::array<Net::Signal, 6> sources{n.input("ARITHMETIC_RESULT"), n.input("LOGIC_RESULT"), n.input("SHIFT_RESULT"), n.input("COMPARE_RESULT"), n.input("A"), n.input("B")};
    std::array<Net::Bits, 6> words;
    for (size_t i = 0; i < words.size(); ++i) words[i] = n.split<8>("RESULT_SOURCE_" + std::to_string(i), sources[i]);
    Net::Bits out;
    for (size_t bit = 0; bit < 8; ++bit) {
        std::vector<Net::Signal> selected;
        for (size_t i = 0; i < sources.size(); ++i)
            selected.push_back(n.binary<ANDGate>("RESULT_SELECT_" + std::to_string(i) + "_" + std::to_string(bit), words[i][bit], enables[i]));
        out[bit] = n.reduceBalanced("RESULT_OR_" + std::to_string(bit), selected, false);
    }
    auto result = n.join<8>("RESULT_JOIN", out);
    n.connect(result, n.output("OUT"));
    n.finish();
}
