#include "modules/composite/ArrayMultiplier.hpp"
#include "modules/composite/MultiplierTree.hpp"
#include "components/GateListPartBehavioral.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/FamilyFactories.hpp"
#include "components/selection/FamilyRegistration.hpp"
#include "modules/rv32im/MulDiv.hpp"
#include "modules/rv32im/ArithmeticGroups.hpp"
#include <stdexcept>

namespace {
void multiplierPins(IOComponent* p, size_t width) {
    if (width != 4 && width != 8 && width != 16 && width != 32)
        throw std::invalid_argument("Array multiplier supports widths 4, 8, 16 and 32");
    p->addPinDynamic("A", PinType::INPUT, width);
    p->addPinDynamic("B", PinType::INPUT, width);
    p->addPin("SIGNED_A", PinType::INPUT);
    p->addPin("SIGNED_B", PinType::INPUT);
    p->addPinDynamic("LO", PinType::OUTPUT, width);
    p->addPinDynamic("HI", PinType::OUTPUT, width);
}
}

ArrayMultiplier::ArrayMultiplier(std::string name, size_t width)
    : IOComponent(std::move(name), [width](IOComponent* p) { multiplierPins(p, width); }),
      width_(width), type_("ArrayMultiplier<" + std::to_string(width) + ">") {}

void ArrayMultiplier::buildInternals(ComponentBuilder& b) {
    using namespace circuit::arithmetic::detail;
    Logic l(b);
    const auto a = l.split(getInputPinDynamic("A"));
    const auto bb = l.split(getInputPinDynamic("B"));
    // An operand read as signed contributes its top bit as the sign bit.
    const auto sign_a = l.land({getInputPin("SIGNED_A"), a.back()});
    const auto sign_b = l.land({getInputPin("SIGNED_B"), bb.back()});
    const auto product = multiplyBits(b, l, a, bb, sign_a, sign_b);
    l.drive(V(product.begin(), product.begin() + width_), getOutputPinDynamic("LO"));
    l.drive(V(product.begin() + width_, product.end()), getOutputPinDynamic("HI"));
    l.finish();
}

namespace {
void mulDivPins(IOComponent* p, size_t width) {
    if (width != 4 && width != 8 && width != 16 && width != 32) throw std::invalid_argument("Invalid arithmetic width");
    for (auto s : {"CLK", "RST", "ENABLE", "CANCEL", "START", "TAKE"}) p->addPin(s, PinType::INPUT);
    for (auto s : {"READY", "BUSY", "DONE"}) p->addPin(s, PinType::OUTPUT);
    for (auto s : {"A", "B"}) p->addPinDynamic(s, PinType::INPUT, width);
    p->addPin<3>("OP", PinType::INPUT);
    p->addPinDynamic("RESULT", PinType::OUTPUT, width);
}

// The multiply half of ArrayMulDivUnit<W>, with the iterative Multiplier<W>'s
// handshake: the launch edge registers the operands, the next edge registers
// the array multiplier's product, and DONE holds it until TAKE.
class ArrayMultiplyUnit : public IOComponent {
    size_t width_;
public:
    ArrayMultiplyUnit(std::string name, size_t width)
        : IOComponent(std::move(name), [width](IOComponent* p) {
              for (auto s : {"CLK", "RST", "ENABLE", "CANCEL", "START", "TAKE", "SIGNED_A", "SIGNED_B"}) p->addPin(s, PinType::INPUT);
              for (auto s : {"READY", "BUSY", "DONE"}) p->addPin(s, PinType::OUTPUT);
              for (auto s : {"A", "B"}) p->addPinDynamic(s, PinType::INPUT, width);
              for (auto s : {"LO", "HI"}) p->addPinDynamic(s, PinType::OUTPUT, width);
          }), width_(width) {}
    const char* getTypeName() const override { return "ArrayMultiplyUnit"; }
    void buildInternals(ComponentBuilder& b) override {
        using circuit::arithmetic::detail::Logic;
        Logic l(b);
        const auto clk = getInputPin("CLK"), rst = getInputPin("RST"), enable = getInputPin("ENABLE"), cancel = getInputPin("CANCEL");
        // The iterative control's rules with a single step: BUSY lasts one edge.
        const auto state_enable = l.lor({enable, cancel});
        auto busy = l.reg("BUSY_STATE", 1, clk, rst, state_enable);
        auto done = l.reg("DONE_STATE", 1, clk, rst, state_enable);
        const auto active = busy->getOutputPin("Q"), valid = done->getOutputPin("Q");
        const auto ready = l.land({l.inv(l.lor({rst, cancel})), l.inv(l.lor({active, valid}))});
        const auto launch = l.land({enable, l.land({getInputPin("START"), ready})});
        l.wire(l.land({l.inv(cancel), launch}), busy->getInputPin("D"));
        l.wire(l.land({l.inv(cancel), l.lor({active, l.land({valid, l.inv(getInputPin("TAKE"))})})}), done->getInputPin("D"));
        auto a = l.reg("OPERAND_A", width_, clk, rst, launch), bb = l.reg("OPERAND_B", width_, clk, rst, launch);
        auto sa = l.reg("SIGNED_A_STATE", 1, clk, rst, launch), sb = l.reg("SIGNED_B_STATE", 1, clk, rst, launch);
        l.wire(getInputPinDynamic("A"), a->getInputPinDynamic("D"));
        l.wire(getInputPinDynamic("B"), bb->getInputPinDynamic("D"));
        l.wire(getInputPin("SIGNED_A"), sa->getInputPin("D"));
        l.wire(getInputPin("SIGNED_B"), sb->getInputPin("D"));
        auto array = b.add(circuit::families::arrayMultiplier(width_), "ARRAY");
        l.wire(a->getOutputPinDynamic("Q"), array->getInputPinDynamic("A"));
        l.wire(bb->getOutputPinDynamic("Q"), array->getInputPinDynamic("B"));
        l.wire(sa->getOutputPin("Q"), array->getInputPin("SIGNED_A"));
        l.wire(sb->getOutputPin("Q"), array->getInputPin("SIGNED_B"));
        const auto write = l.land({enable, active});
        auto lo = l.reg("PRODUCT_LOW", width_, clk, rst, write), hi = l.reg("PRODUCT_HIGH", width_, clk, rst, write);
        l.wire(array->getOutputPinDynamic("LO"), lo->getInputPinDynamic("D"));
        l.wire(array->getOutputPinDynamic("HI"), hi->getInputPinDynamic("D"));
        l.wire(lo->getOutputPinDynamic("Q"), getOutputPinDynamic("LO"));
        l.wire(hi->getOutputPinDynamic("Q"), getOutputPinDynamic("HI"));
        l.out(ready, getOutputPin("READY"));
        l.out(active, getOutputPin("BUSY"));
        l.out(valid, getOutputPin("DONE"));
        l.finish();
    }
};
}

ArrayMulDivUnit::ArrayMulDivUnit(std::string name, size_t width)
    : IOComponent(std::move(name), [width](IOComponent* p) { mulDivPins(p, width); }),
      width_(width), type_("ArrayMulDivUnit<" + std::to_string(width) + ">") {}

// MulDivUnit<W>'s assembly with the array multiply unit in place of the
// iterative multiplier; division stays iterative.
void ArrayMulDivUnit::buildInternals(ComponentBuilder& b) {
    using circuit::arithmetic::detail::Logic;
    Logic l(b);
    auto mul = b.addNewComponent<ArrayMultiplyUnit>("MULTIPLY", width_);
    auto div = b.add(circuit::families::divider(width_), "DIVIDE");
    for (auto s : {"CLK", "RST", "ENABLE", "CANCEL", "A", "B", "TAKE"})
        for (const auto& u : {std::static_pointer_cast<IOComponent>(mul), div}) l.wire(getInputPinDynamic(s), u->getInputPinDynamic(s));
    auto ready = l.land({mul->getOutputPin("READY"), div->getOutputPin("READY")});
    auto start = l.land({getInputPin("ENABLE"), l.land({getInputPin("START"), ready})});
    auto op = l.split(getInputPin<3>("OP"));
    auto saved = l.reg("OPERATION", 3, getInputPin("CLK"), getInputPin("RST"), start);
    l.wire(getInputPin<3>("OP"), saved->getInputPin<3>("D"));
    l.wire(l.land({start, l.inv(op[2])}), mul->getInputPin("START"));
    l.wire(l.land({start, op[2]}), div->getInputPin("START"));
    l.wire(l.lx({op[0], op[1]}), mul->getInputPin("SIGNED_A"));
    l.wire(l.land({op[0], l.inv(op[1])}), mul->getInputPin("SIGNED_B"));
    l.wire(l.inv(op[0]), div->getInputPin("SIGNED"));
    auto select = b.addNewComponent<rv32im::detail::MulDivResultSelection>("RESULT_SELECTION", width_);
    l.wire(saved->getOutputPin<3>("Q"), select->getInputPin<3>("OP"));
    for (auto s : {"LO", "HI"}) l.wire(mul->getOutputPinDynamic(s), select->getInputPinDynamic(s));
    for (auto s : {"QUOTIENT", "REMAINDER"}) l.wire(div->getOutputPinDynamic(s), select->getInputPinDynamic(s));
    l.wire(select->getOutputPinDynamic("RESULT"), getOutputPinDynamic("RESULT"));
    l.out(ready, getOutputPin("READY"));
    l.out(l.lor({mul->getOutputPin("BUSY"), div->getOutputPin("BUSY")}), getOutputPin("BUSY"));
    l.out(l.lor({mul->getOutputPin("DONE"), div->getOutputPin("DONE")}), getOutputPin("DONE"));
    l.finish();
}

namespace circuit::families {
namespace {
template<size_t W> const ComponentFamily& mulDivFamily() {
    static const std::string id = "arithmetic.array-mul-div." + std::to_string(W);
    static const std::string type = "ArrayMulDivUnit<" + std::to_string(W) + ">";
    static const ComponentFamily f{id, type, [](IOComponent* p) { mulDivPins(p, W); },
        [](const std::string& n, const std::shared_ptr<BuildContext>& c) { return Component::createWithContext<::ArrayMulDivUnit>(c, n, W); },
        [](const std::string& n, const std::shared_ptr<BuildContext>& c) { return circuit::detail::createGateListPart(mulDivFamily<W>(), n, c); }};
    return f;
}
template<size_t W> const ComponentFamily& arrayFamily() {
    static const std::string id = "arithmetic.array-multiplier." + std::to_string(W);
    static const std::string type = "ArrayMultiplier<" + std::to_string(W) + ">";
    static const ComponentFamily f{id, type, [](IOComponent* p) { multiplierPins(p, W); },
        [](const std::string& n, const std::shared_ptr<BuildContext>& c) { return Component::createWithContext<::ArrayMultiplier>(c, n, W); },
        [](const std::string& n, const std::shared_ptr<BuildContext>& c) { return circuit::detail::createGateListPart(arrayFamily<W>(), n, c); }};
    return f;
}
}
const ComponentFamily& arrayMultiplier(size_t width) {
    switch (width) {
    case 4: return arrayFamily<4>();
    case 8: return arrayFamily<8>();
    case 16: return arrayFamily<16>();
    case 32: return arrayFamily<32>();
    }
    throw std::invalid_argument("Invalid array multiplier width");
}
const ComponentFamily& arrayMulDivUnit(size_t width) {
    switch (width) {
    case 4: return mulDivFamily<4>();
    case 8: return mulDivFamily<8>();
    case 16: return mulDivFamily<16>();
    case 32: return mulDivFamily<32>();
    }
    throw std::invalid_argument("Invalid array mul-div width");
}
}

namespace circuit::arithmetic {
void registerArrayMultipliers(ComponentCatalog& c) {
    for (size_t w : {4u, 8u, 16u, 32u}) {
        circuit::registration::registerStructural(c, circuit::families::arrayMultiplier(w), "ArrayMultiplierTest");
        circuit::registration::registerBehavioral(c, circuit::families::arrayMultiplier(w), "ArrayMultiplierTest",
            "ArrayMultiplierTest", {}, "stable-after-settle");
        circuit::registration::registerStructural(c, circuit::families::arrayMulDivUnit(w), "ArrayMulDivUnitTest", "clock-boundary");
        circuit::registration::registerBehavioral(c, circuit::families::arrayMulDivUnit(w), "ArrayMulDivUnitTest", "ArrayMulDivUnitTest");
    }
}
}
