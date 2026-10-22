#include "components/ComponentBuilder.tpp"
#include "modules/composite/WordBuilder.hpp"
namespace circuit::word {
void buildALU(IOComponent& self,ComponentBuilder& builder,size_t width,ALUConfig config) {
    WordBuilder n(self, builder, width, config);
    auto control = n.add(controlFamily(config.operations), "CONTROL");
    auto arith = n.addWord(Unit::Arithmetic, "ARITHMETIC");
    auto logical = n.addWord(Unit::Logic, "LOGIC");
    auto shifter = n.addWord(Unit::Shift, "SHIFTER");
    auto comparison = n.addWord(Unit::Compare, "COMPARE_SELECT");
    auto selector = n.addWord(Unit::Result, "RESULT_SELECT");
    auto flags = n.addWord(Unit::Flags, "FLAGS");
    n.connect(n.input("OP"), control->getInputPin<5>("OP"));
    for (const auto& unit : std::vector<std::shared_ptr<IOComponent>>{arith, logical, shifter, comparison, selector}) {
        n.connect(n.input("A"), unit->getInputPinDynamic("A"));
        n.connect(n.input("B"), unit->getInputPinDynamic("B"));
    }
    n.connect(n.input("C_IN"), arith->getInputPin("C_IN"));
    n.connect(n.input("C_IN"), shifter->getInputPin("C_IN"));
    n.connect(control->getOutputPin<3>("ARITH_OP"), arith->getInputPin<3>("OP"));
    n.connect(control->getOutputPin<3>("LOCAL_OP"), logical->getInputPin<3>("OP"));
    n.connect(control->getOutputPin<3>("LOCAL_OP"), shifter->getInputPin<3>("OP"));
    n.connect(control->getOutputPin<3>("COMPARE_OP"), comparison->getInputPin<3>("OP"));
    n.connect(arith->getOutputPinDynamic("OUT"), comparison->getInputPinDynamic("DIFF"));
    n.connect(arith->getOutputPin("CARRY"), comparison->getInputPin("CARRY_IN"));
    n.connect(arith->getOutputPin("OVERFLOW"), comparison->getInputPin("OVERFLOW_IN"));
    n.connect(arith->getOutputPinDynamic("OUT"), selector->getInputPinDynamic("ARITHMETIC_RESULT"));
    n.connect(logical->getOutputPinDynamic("OUT"), selector->getInputPinDynamic("LOGIC_RESULT"));
    n.connect(shifter->getOutputPinDynamic("OUT"), selector->getInputPinDynamic("SHIFT_RESULT"));
    n.connect(comparison->getOutputPinDynamic("OUT"), selector->getInputPinDynamic("COMPARE_RESULT"));
    for (const auto* enable : {"ARITHMETIC_ENABLE", "LOGIC_ENABLE", "SHIFT_ENABLE",
                               "COMPARE_ENABLE", "PASS_A_ENABLE", "PASS_B_ENABLE"})
        n.connect(control->getOutputPin(enable), selector->getInputPin(enable));
    n.connect(selector->getOutputPinDynamic("OUT"), n.output("OUT"));
    n.connect(selector->getOutputPinDynamic("OUT"), flags->getInputPinDynamic("RESULT"));
    n.connect(control->getOutputPin("ARITHMETIC_ENABLE"), flags->getInputPin("ARITHMETIC"));
    n.connect(control->getOutputPin("SHIFT_ENABLE"), flags->getInputPin("SHIFT"));
    n.connect(arith->getOutputPin("CARRY"), flags->getInputPin("ARITH_CARRY"));
    n.connect(arith->getOutputPin("OVERFLOW"), flags->getInputPin("ARITH_OVERFLOW"));
    n.connect(shifter->getOutputPin("CARRY"), flags->getInputPin("SHIFT_CARRY"));
    for (const auto* name : {"ZERO", "NEGATIVE", "CARRY", "OVERFLOW"})
        n.connect(flags->getOutputPin(name), n.output(name));
    n.connect(control->getOutputPin("INVALID_OP"), n.output("INVALID_OP"));
    n.finish();
}

void buildArithmetic(IOComponent& self,ComponentBuilder& builder,size_t width,ALUConfig config) {
    WordBuilder n(self, builder, width, config);
    auto lo = n.constant<1>("LOW", 0), hi = n.constant<1>("HIGH", 1);
    auto zero = n.constantWord("ZERO"+std::to_string(width), 0), one = n.constantWord("ONE"+std::to_string(width), 1);
    auto a = n.input("A"), b = n.input("B"), op = n.input("OP"), cin = n.input("C_IN");
    auto core = n.addWord(Unit::AddSub, "ADD_SUB");
    n.connect(n.selectBus<8>("X_SELECT", {a,a,a,a,a,a,zero,zero}, op), core->getInputPinDynamic("A"));
    n.connect(n.selectBus<8>("Y_SELECT", {b,b,b,b,zero,one,a,zero}, op), core->getInputPinDynamic("B"));
    n.connect(n.selectBit<8>("SUB_SELECT", {lo,lo,hi,hi,lo,hi,hi,lo}, op), core->getInputPin("SUB"));
    n.connect(n.selectBit<8>("CARRY_IN_SELECT", {lo,cin,hi,cin,hi,hi,hi,lo}, op), core->getInputPin("C_IN"));
    auto invalid = n.selectBit<8>("INVALID_SELECT", {lo,lo,lo,lo,lo,lo,lo,hi}, op);
    auto valid = n.invert("VALID", invalid);
    n.connect(invalid, n.output("INVALID_OP"));
    n.connect(n.maskBus("RESULT_MASK", core->getOutputPinDynamic("OUT"), valid), n.output("OUT"));
    n.connect(n.binary<ANDGate>("CARRY_MASK", core->getOutputPin("CARRY_OUT"), valid), n.output("CARRY"));
    n.connect(n.binary<ANDGate>("OVERFLOW_MASK", core->getOutputPin("OVERFLOW"), valid), n.output("OVERFLOW"));
    n.finish();
}

void buildLogic(IOComponent& self,ComponentBuilder& builder,size_t width,ALUConfig config) {
    WordBuilder n(self, builder, width, config);
    auto a = n.splitWord("A_SPLIT", n.input("A")), b = n.splitWord("B_SPLIT", n.input("B"));
    auto zero = n.constant<1>("ZERO", 0);
    WordBuilder::Bits result(width);
    for (size_t i = 0; i < width; ++i) {
        const auto suffix = "_" + std::to_string(i);
        auto and_bit = n.binary<ANDGate>("AND" + suffix, a[i], b[i]);
        auto or_bit = n.binary<ORGate>("OR" + suffix, a[i], b[i]);
        auto xor_bit = n.binary<XORGate>("XOR" + suffix, a[i], b[i]);
        result[i] = n.selectBit<8>("SELECT" + suffix,
            {and_bit, or_bit, xor_bit, n.invert("NOT" + suffix, a[i]),
             n.invert("NAND" + suffix, and_bit), n.invert("NOR" + suffix, or_bit),
             n.invert("XNOR" + suffix, xor_bit), zero}, n.input("OP"));
    }
    n.connect(n.joinWord("OUT_JOIN", result), n.output("OUT"));
    n.finish();
}

void buildShift(IOComponent& self,ComponentBuilder& builder,size_t width,ALUConfig config) {
    WordBuilder n(self, builder, width, config);
    auto a = n.splitWord("A_SPLIT", n.input("A")), b = n.splitWord("B_SPLIT", n.input("B"));
    auto lo = n.constant<1>("LOW", 0), hi = n.constant<1>("HIGH", 1), op = n.input("OP");
    auto left = n.selectBit<8>("LEFT_SELECT", {hi,lo,lo,hi,lo,lo,lo,lo}, op);
    auto rotate = n.selectBit<8>("ROTATE_SELECT", {lo,lo,lo,hi,hi,lo,lo,lo}, op);
    auto arithmetic = n.selectBit<8>("ARITHMETIC_SELECT", {lo,lo,hi,lo,lo,lo,lo,lo}, op);
    auto valid = n.selectBit<8>("VALID_SELECT", {hi,hi,hi,hi,hi,lo,lo,lo}, op);
    auto fill = n.binary<ANDGate>("SIGN_FILL", a[width-1], arithmetic);
    WordBuilder::Bits current(width);
    for (size_t i = 0; i < width; ++i)
        current[i] = n.mux("INPUT_REVERSE_" + std::to_string(i), a[i], a[width-1-i], left);
    auto carry = n.input("C_IN");
    for (size_t stage = 0; stage < std::countr_zero(width); ++stage) {
        const size_t distance = size_t{1} << stage;
        const auto prefix = "SHIFT_" + std::to_string(distance) + "_";
        auto previous = current;
        carry = n.mux(prefix + "CARRY", carry, previous[distance-1], b[stage]);
        for (size_t i = 0; i < width; ++i) {
            auto moved = i + distance < width ? previous[i+distance]
                : n.mux(prefix + "FILL_" + std::to_string(i), fill, previous[i+distance-width], rotate);
            current[i] = n.mux(prefix + "BIT_" + std::to_string(i), previous[i], moved, b[stage]);
        }
    }
    auto shifted = current;
    for (size_t i = 0; i < width; ++i) {
        current[i] = n.mux("OUTPUT_REVERSE_" + std::to_string(i), shifted[i], shifted[width-1-i], left);
        current[i] = n.binary<ANDGate>("VALID_BIT_" + std::to_string(i), current[i], valid);
    }
    n.connect(n.joinWord("OUT_JOIN", current), n.output("OUT"));
    n.connect(n.binary<ANDGate>("CARRY_VALID", carry, valid), n.output("CARRY"));
    n.finish();
}

void buildCompare(IOComponent& self,ComponentBuilder& builder,size_t width,ALUConfig config) {
    WordBuilder n(self, builder, width, config);
    auto diff = n.splitWord("DIFF_SPLIT", n.input("DIFF"));
    auto zero = n.addWord(Unit::Zero, "EQUAL");
    n.connect(n.input("DIFF"), zero->getInputPinDynamic("A"));
    auto ltu = n.invert("UNSIGNED_LT", n.input("CARRY_IN"));
    auto lts = n.binary<XORGate>("SIGNED_LT", diff[width-1], n.input("OVERFLOW_IN"));
    auto lo = n.constant<1>("ZERO", 0);
    auto a = n.splitWord("A_SPLIT", n.input("A")), b = n.splitWord("B_SPLIT", n.input("B"));
    WordBuilder::Bits out(width);
    for (size_t i = 0; i < width; ++i) {
        auto min_u = n.mux("MINU_" + std::to_string(i), b[i], a[i], ltu);
        auto max_u = n.mux("MAXU_" + std::to_string(i), a[i], b[i], ltu);
        auto min_s = n.mux("MINS_" + std::to_string(i), b[i], a[i], lts);
        auto max_s = n.mux("MAXS_" + std::to_string(i), a[i], b[i], lts);
        out[i] = n.selectBit<8>("RESULT_" + std::to_string(i),
            {i == 0 ? zero->getOutputPin("ZERO") : lo, i == 0 ? ltu : lo, i == 0 ? lts : lo,
             min_u, max_u, min_s, max_s, lo}, n.input("OP"));
    }
    n.connect(n.joinWord("OUT_JOIN", out), n.output("OUT"));
    n.finish();
}

void buildFlags(IOComponent& self,ComponentBuilder& builder,size_t width,ALUConfig config) {
    WordBuilder n(self, builder, width, config);
    auto zero = n.addWord(Unit::Zero, "ZERO_DETECT");
    n.connect(n.input("RESULT"), zero->getInputPinDynamic("A"));
    n.connect(zero->getOutputPin("ZERO"), n.output("ZERO"));
    auto result = n.splitWord("RESULT_SPLIT", n.input("RESULT"));
    auto low = n.constant<1>("LOW", 0);
    n.connect(n.binary<ORGate>("SIGN", result[width-1], low), n.output("NEGATIVE"));
    auto arithmetic_carry = n.binary<ANDGate>("ARITH_CARRY_MASK", n.input("ARITHMETIC"), n.input("ARITH_CARRY"));
    auto shift_carry = n.binary<ANDGate>("SHIFT_CARRY_MASK", n.input("SHIFT"), n.input("SHIFT_CARRY"));
    n.connect(n.binary<ORGate>("CARRY_SELECT", arithmetic_carry, shift_carry), n.output("CARRY"));
    n.connect(n.binary<ANDGate>("OVERFLOW_SELECT", n.input("ARITHMETIC"), n.input("ARITH_OVERFLOW")), n.output("OVERFLOW"));
    n.finish();
}

void buildResult(IOComponent& self,ComponentBuilder& builder,size_t width,ALUConfig config) {
    using Net = WordBuilder;
    Net n(self, builder, width, config);
    std::array<Net::Signal, 6> enables{n.input("ARITHMETIC_ENABLE"), n.input("LOGIC_ENABLE"), n.input("SHIFT_ENABLE"), n.input("COMPARE_ENABLE"), n.input("PASS_A_ENABLE"), n.input("PASS_B_ENABLE")};
    std::array<Net::Signal, 6> sources{n.input("ARITHMETIC_RESULT"), n.input("LOGIC_RESULT"), n.input("SHIFT_RESULT"), n.input("COMPARE_RESULT"), n.input("A"), n.input("B")};
    std::array<Net::Bits, 6> words;
    for (size_t i = 0; i < words.size(); ++i) words[i] = n.splitWord("RESULT_SOURCE_" + std::to_string(i), sources[i]);
    Net::Bits out(width);
    for (size_t bit = 0; bit < width; ++bit) {
        std::vector<Net::Signal> selected;
        for (size_t i = 0; i < sources.size(); ++i)
            selected.push_back(n.binary<ANDGate>("RESULT_SELECT_" + std::to_string(i) + "_" + std::to_string(bit), words[i][bit], enables[i]));
        out[bit] = n.reduceBalanced("RESULT_OR_" + std::to_string(bit), selected, false);
    }
    auto result = n.joinWord("RESULT_JOIN", out);
    n.connect(result, n.output("OUT"));
    n.finish();
}

void buildAddSub(IOComponent& self,ComponentBuilder& builder,size_t width,ALUConfig config) {
    WordBuilder n(self, builder, width, config);
    auto a = n.splitWord("A_SPLIT", n.input("A"));
    auto b = n.splitWord("B_SPLIT", n.input("B"));
    for (size_t i = 0; i < width; ++i)
        b[i] = n.binary<XORGate>("B_XOR_SUB_" + std::to_string(i), b[i], n.input("SUB"));
    auto adder = n.addWord(Unit::Adder, "ADD");
    n.connect(n.input("A"), adder->getInputPinDynamic("A"));
    n.connect(n.joinWord("B_JOIN", b), adder->getInputPinDynamic("B"));
    n.connect(n.input("C_IN"), adder->getInputPin("Cin"));
    auto result = adder->getOutputPinDynamic("Sum");
    auto r = n.splitWord("RESULT_SPLIT", result);
    auto same_sign = n.invert("SAME_SIGN", n.binary<XORGate>("INPUT_SIGNS", a[width-1], b[width-1]));
    auto sign_changed = n.binary<XORGate>("RESULT_SIGN_CHANGED", a[width-1], r[width-1]);
    n.connect(n.binary<ANDGate>("OVERFLOW_AND", same_sign, sign_changed), n.output("OVERFLOW"));
    n.connect(result, n.output("OUT"));
    n.connect(adder->getOutputPin("Cout"), n.output("CARRY_OUT"));
    n.finish();
}
}
