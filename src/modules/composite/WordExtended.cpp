#include "components/ComponentBuilder.tpp"
#include "modules/composite/WordBuilder.hpp"
namespace circuit::word {
void buildALU(IOComponent& self,ComponentBuilder& builder,size_t width) {
    WordBuilder n(self, builder, width);
    auto control = n.add(controlFamily(), "CONTROL");
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

void buildArithmetic(IOComponent& self,ComponentBuilder& builder,size_t width) {
    WordBuilder n(self, builder, width);
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
    n.connect(n.binary<ANDGate>("CARRY_MASK", core->getOutputPin("C_OUT"), valid), n.output("CARRY"));
    n.connect(n.binary<ANDGate>("OVERFLOW_MASK", core->getOutputPin("OVERFLOW"), valid), n.output("OVERFLOW"));
    n.finish();
}

// Word gates: the same per-bit AND/OR/XOR, inverters and 8-way select as a
// bit-by-bit circuit, grouped as words (Mux<8,W> is a Mux<8,1> per bit).
void buildLogic(IOComponent& self,ComponentBuilder& builder,size_t width) {
    using circuit::word::GateKind;
    WordBuilder n(self, builder, width);
    const auto a = n.input("A"), b = n.input("B");
    const auto and_word = n.wordGate(GateKind::And, "AND", a, b);
    const auto or_word = n.wordGate(GateKind::Or, "OR", a, b);
    const auto xor_word = n.wordGate(GateKind::Xor, "XOR", a, b);
    const auto result = n.selectBus<8>("SELECT",
        {and_word, or_word, xor_word, n.wordGate(GateKind::Not, "NOT", a),
         n.wordGate(GateKind::Not, "NAND", and_word), n.wordGate(GateKind::Not, "NOR", or_word),
         n.wordGate(GateKind::Not, "XNOR", xor_word), n.constantWord("ZERO", 0)}, n.input("OP"));
    n.connect(result, n.output("OUT"));
    n.finish();
}

void buildShift(IOComponent& self,ComponentBuilder& builder,size_t width) {
    using circuit::word::GateKind;
    WordBuilder n(self, builder, width);
    auto a = n.splitWord("A_SPLIT", n.input("A")), b = n.splitWord("B_SPLIT", n.input("B"));
    auto lo = n.constant<1>("LOW", 0), hi = n.constant<1>("HIGH", 1), op = n.input("OP");
    auto left = n.selectBit<8>("LEFT_SELECT", {hi,lo,lo,hi,lo,lo,lo,lo}, op);
    auto rotate = n.selectBit<8>("ROTATE_SELECT", {lo,lo,lo,hi,hi,lo,lo,lo}, op);
    auto arithmetic = n.selectBit<8>("ARITHMETIC_SELECT", {lo,lo,hi,lo,lo,lo,lo,lo}, op);
    auto valid = n.selectBit<8>("VALID_SELECT", {hi,hi,hi,hi,hi,lo,lo,lo}, op);
    auto fill = n.binary<ANDGate>("SIGN_FILL", a[width-1], arithmetic);
    const auto reversed = [&](const std::string& name, const WordBuilder::Bits& bits) {
        return n.joinWord(name, WordBuilder::Bits(bits.rbegin(), bits.rend()));
    };
    // Reverse for left shifts, shift right in log2(W) stages, reverse back.
    // Each step is one 2:1 word mux (a Mux<2,1> per bit); reversing and
    // moving a word is wiring only.
    auto current = n.wordMux("INPUT_REVERSE", n.input("A"), reversed("A_REVERSED", a), left);
    auto carry = n.input("C_IN");
    for (size_t stage = 0; stage < std::countr_zero(width); ++stage) {
        const size_t distance = size_t{1} << stage;
        const auto prefix = "SHIFT_" + std::to_string(distance);
        const auto previous = n.splitWord(prefix + "_SPLIT", current);
        carry = n.mux(prefix + "_CARRY", carry, previous[distance-1], b[stage]);
        // The top `distance` bits take the fill bit, or for rotation the bits
        // shifted out at the bottom: one mux per bit, a word mux from 4 bits.
        WordBuilder::Bits wrapped(distance);
        if (distance >= 4) {
            WordBuilder::Bits shifted_out(previous.begin(), previous.begin() + distance);
            auto word = n.wordMux(prefix + "_FILL", n.broadcast(prefix + "_FILL_WORD", fill, distance),
                                  n.joinBits(prefix + "_OUT_JOIN", shifted_out), rotate);
            wrapped = n.splitBits(prefix + "_FILL_SPLIT", word);
        } else {
            for (size_t i = 0; i < distance; ++i)
                wrapped[i] = n.mux(prefix + "_FILL_" + std::to_string(width - distance + i), fill, previous[i], rotate);
        }
        WordBuilder::Bits moved(width);
        for (size_t i = 0; i < width; ++i) moved[i] = i + distance < width ? previous[i+distance] : wrapped[i + distance - width];
        current = n.wordMux(prefix, current, n.joinWord(prefix + "_MOVED", moved), b[stage]);
    }
    auto shifted = n.splitWord("SHIFTED_SPLIT", current);
    auto out = n.wordMux("OUTPUT_REVERSE", current, reversed("SHIFTED_REVERSED", shifted), left);
    n.connect(n.maskBus("VALID", out, valid), n.output("OUT"));
    n.connect(n.binary<ANDGate>("CARRY_VALID", carry, valid), n.output("CARRY"));
    n.finish();
}

void buildCompare(IOComponent& self,ComponentBuilder& builder,size_t width) {
    WordBuilder n(self, builder, width);
    auto diff = n.splitWord("DIFF_SPLIT", n.input("DIFF"));
    auto zero = n.addWord(Unit::Zero, "EQUAL");
    n.connect(n.input("DIFF"), zero->getInputPinDynamic("A"));
    auto ltu = n.invert("UNSIGNED_LT", n.input("CARRY_IN"));
    auto lts = n.binary<XORGate>("SIGNED_LT", diff[width-1], n.input("OVERFLOW_IN"));
    auto lo = n.constant<1>("ZERO", 0);
    auto a = n.input("A"), b = n.input("B");
    // Min/max are 2:1 word muxes; the flag results are words with the flag in
    // bit 0. One Mux<8,W> (an 8-way mux per bit) picks the result.
    const auto flag_word = [&](const std::string& name, WordBuilder::Signal flag) {
        WordBuilder::Bits bits(width, lo);
        bits[0] = flag;
        return n.joinWord(name, bits);
    };
    auto min_u = n.wordMux("MINU", b, a, ltu), max_u = n.wordMux("MAXU", a, b, ltu);
    auto min_s = n.wordMux("MINS", b, a, lts), max_s = n.wordMux("MAXS", a, b, lts);
    auto result = n.selectBus<8>("RESULT",
        {flag_word("EQUAL_WORD", zero->getOutputPin("ZERO")), flag_word("LTU_WORD", ltu), flag_word("LTS_WORD", lts),
         min_u, max_u, min_s, max_s, flag_word("ZERO_WORD", lo)}, n.input("OP"));
    n.connect(result, n.output("OUT"));
    n.finish();
}

void buildFlags(IOComponent& self,ComponentBuilder& builder,size_t width) {
    WordBuilder n(self, builder, width);
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

void buildResult(IOComponent& self,ComponentBuilder& builder,size_t width) {
    using Net = WordBuilder;
    Net n(self, builder, width);
    std::array<Net::Signal, 6> enables{n.input("ARITHMETIC_ENABLE"), n.input("LOGIC_ENABLE"), n.input("SHIFT_ENABLE"), n.input("COMPARE_ENABLE"), n.input("PASS_A_ENABLE"), n.input("PASS_B_ENABLE")};
    std::array<Net::Signal, 6> sources{n.input("ARITHMETIC_RESULT"), n.input("LOGIC_RESULT"), n.input("SHIFT_RESULT"), n.input("COMPARE_RESULT"), n.input("A"), n.input("B")};
    // Each source word ANDed with its enable (spread by wiring), then a
    // balanced OR tree of words: the same gates per bit as a one-hot AND/OR mux.
    std::vector<Net::Signal> selected;
    for (size_t i = 0; i < sources.size(); ++i)
        selected.push_back(n.maskBus("RESULT_SELECT_" + std::to_string(i), sources[i], enables[i]));
    n.connect(n.reduceWords("RESULT_OR", selected, circuit::word::GateKind::Or), n.output("OUT"));
    n.finish();
}

void buildAddSub(IOComponent& self,ComponentBuilder& builder,size_t width) {
    WordBuilder n(self, builder, width);
    auto a = n.splitWord("A_SPLIT", n.input("A"));
    // B XOR SUB on every bit: one word XOR with SUB spread across the word.
    const auto effective_b = n.wordGate(circuit::word::GateKind::Xor, "B_XOR_SUB", n.input("B"),
                                        n.broadcast("SUB_WORD", n.input("SUB"), width));
    auto b = n.splitWord("B_SPLIT", effective_b);
    auto adder = n.addWord(Unit::PrefixAdder, "ADD");
    n.connect(n.input("A"), adder->getInputPinDynamic("A"));
    n.connect(effective_b, adder->getInputPinDynamic("B"));
    n.connect(n.input("C_IN"), adder->getInputPin("C_IN"));
    auto result = adder->getOutputPinDynamic("OUT");
    auto r = n.splitWord("RESULT_SPLIT", result);
    auto same_sign = n.invert("SAME_SIGN", n.binary<XORGate>("INPUT_SIGNS", a[width-1], b[width-1]));
    auto sign_changed = n.binary<XORGate>("RESULT_SIGN_CHANGED", a[width-1], r[width-1]);
    n.connect(n.binary<ANDGate>("OVERFLOW_AND", same_sign, sign_changed), n.output("OVERFLOW"));
    n.connect(result, n.output("OUT"));
    n.connect(adder->getOutputPin("C_OUT"), n.output("C_OUT"));
    n.finish();
}
}
