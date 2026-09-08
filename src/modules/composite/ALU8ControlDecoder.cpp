#include "modules/composite/ALU8ControlDecoder.hpp"
#include "modules/composite/Structural8Builder.hpp"
#include "components/PinMacros.hpp"
#include "modules/basic/Decoder.hpp"

ALU8ControlDecoder::ALU8ControlDecoder(std::string name)
    : IOComponent(std::move(name), circuit::families::ALU8ControlDecoder.pinInitializer()) {}

void ALU8ControlDecoder::buildInternals(ComponentBuilder& builder) {
    using Net = circuit::detail::Structural8Builder;
    Net n(*this, builder);
    auto decoder = n.add<Decoder5to32>("OPERATION_DECODER");
    n.connect(n.input("OP"), decoder->getInputPin<5>("ADDR"));
    n.connect(n.constant<1>("HIGH", 1), decoder->getInputPin("ENABLE"));
    auto decoded = [&](size_t i) -> Net::Signal { return decoder->getOutputPin("OUT" + std::to_string(i)); };
    auto group = [&](const std::string& name, size_t begin, size_t end) {
        std::vector<Net::Signal> bits;
        for (size_t i = begin; i <= end; ++i) bits.push_back(decoded(i));
        return n.any(name, bits);
    };
    auto arithmetic = group("ARITHMETIC_ENABLE", 0, 6);
    auto logic = group("LOGIC_ENABLE", 8, 14);
    auto shift = group("SHIFT_ENABLE", 16, 20);
    auto compare = group("COMPARE_ENABLE", 21, 27);
    n.connect(n.any("INVALID_DECODE", {decoded(7), decoded(15), decoded(31)}), n.output("INVALID_OP"));
    auto op_bits = n.split<5>("OP_SPLIT", n.input("OP"));
    std::array<Net::Signal, 3> low_op{op_bits[0], op_bits[1], op_bits[2]};
    auto local_op = n.join<3>("LOCAL_OP", low_op);

    // Compare operations reuse the same subtraction hardware as SUB/SBC.
    auto low = n.constant<1>("LOW", 0), high = n.constant<1>("SUB_BIT", 1);
    std::array<Net::Signal, 3> arithmetic_op;
    for (size_t i = 0; i < 3; ++i)
        arithmetic_op[i] = n.mux("ARITH_OP_" + std::to_string(i), low_op[i], i == 1 ? high : low, compare);
    n.connect(n.join<3>("ARITH_OP_JOIN", arithmetic_op), n.output("ARITH_OP"));
    std::array<Net::Signal, 3> compare_op;
    for (size_t bit = 0; bit < 3; ++bit) {
        std::vector<Net::Signal> sources;
        for (size_t i = 0; i < 7; ++i) if ((i >> bit) & 1) sources.push_back(decoded(21+i));
        compare_op[bit] = n.any("COMPARE_OP_" + std::to_string(bit), sources);
    }
    n.connect(n.join<3>("COMPARE_OP_JOIN", compare_op), n.output("COMPARE_OP"));
    n.connect(local_op, n.output("LOCAL_OP"));
    n.connect(arithmetic, n.output("ARITHMETIC_ENABLE"));
    n.connect(logic, n.output("LOGIC_ENABLE"));
    n.connect(shift, n.output("SHIFT_ENABLE"));
    n.connect(compare, n.output("COMPARE_ENABLE"));
    n.connect(decoded(28), n.output("PASS_A_ENABLE"));
    n.connect(decoded(29), n.output("PASS_B_ENABLE"));
    n.finish();
}
