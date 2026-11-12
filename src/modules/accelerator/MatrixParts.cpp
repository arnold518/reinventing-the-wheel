#include "modules/accelerator/MatrixParts.hpp"
#include "modules/composite/MultiplierTree.hpp"
#include "modules/basic/Decoder.hpp"
#include "components/GateListPartBehavioral.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/FamilyFactories.hpp"
#include "components/selection/FamilyRegistration.hpp"
#include "components/capabilities/BehavioralComposition.hpp"
#include "components/selection/GateListCompact.hpp"
#include "components/BasicComponent.hpp"
#include <stdexcept>

using circuit::arithmetic::detail::Logic;
using circuit::arithmetic::detail::P;
using circuit::arithmetic::detail::V;
using circuit::arithmetic::detail::multiplyBits;
using circuit::arithmetic::detail::addRows;

namespace {
void cellPins(IOComponent* p) {
    for (auto n : {"CLK", "RST", "SHIFT"}) p->addPin(n, PinType::INPUT);
    p->addPin<8>("W_IN", PinType::INPUT);
    p->addPin<8>("X_IN", PinType::INPUT);
    p->addPin<32>("SUM_IN", PinType::INPUT);
    p->addPin<8>("W_OUT", PinType::OUTPUT);
    p->addPin<8>("X_OUT", PinType::OUTPUT);
    p->addPin<32>("SUM_OUT", PinType::OUTPUT);
}
void requantizerPins(IOComponent* p) {
    p->addPin<32>("ACC", PinType::INPUT);
    p->addPin<16>("SCALE", PinType::INPUT);
    p->addPin<5>("SHIFT", PinType::INPUT);
    p->addPin("RELU", PinType::INPUT);
    p->addPin<8>("OUT", PinType::OUTPUT);
}
void checkArraySize(size_t n) {
    if (n != 2 && n != 4 && n != 8 && n != 16) throw std::invalid_argument("Systolic array size must be 2, 4, 8 or 16");
}
void arrayPins(IOComponent* p, size_t n) {
    checkArraySize(n);
    for (auto s : {"CLK", "RST", "SHIFT"}) p->addPin(s, PinType::INPUT);
    for (size_t j = 0; j < n; ++j) p->addPin<8>("W_IN_" + std::to_string(j), PinType::INPUT);
    for (size_t i = 0; i < n; ++i) p->addPin<8>("X_IN_" + std::to_string(i), PinType::INPUT);
    for (size_t j = 0; j < n; ++j) p->addPin<32>("Y_OUT_" + std::to_string(j), PinType::OUTPUT);
}
}

namespace circuit::accelerator {

MacCell::MacCell(std::string name) : IOComponent(std::move(name), cellPins) {}

void MacCell::buildInternals(ComponentBuilder& b) {
    Logic l(b);
    const auto clk = getInputPin("CLK"), rst = getInputPin("RST");
    auto weight = l.reg("WEIGHT", 8, clk, rst, getInputPin("SHIFT"));
    l.wire(getInputPin<8>("W_IN"), weight->getInputPinDynamic("D"));
    l.wire(weight->getOutputPinDynamic("Q"), getOutputPin<8>("W_OUT"));
    // int8 × int8 → int16, sign-extended to 32 bits and added to the sum from above.
    const auto x = l.split(getInputPin<8>("X_IN"));
    const auto wbits = l.split(weight->getOutputPinDynamic("Q"));
    auto product = multiplyBits(b, l, x, wbits, x.back(), wbits.back());
    const auto sign = product.back();
    while (product.size() < 32) product.push_back(sign);
    const auto [sum, carry] = l.addToWord(getInputPin<32>("SUM_IN"), product, false);
    (void)carry;   // int32 accumulation wraps
    auto sum_reg = l.reg("SUM", 32, clk, rst, l.bit(true));
    l.wire(sum, sum_reg->getInputPinDynamic("D"));
    l.wire(sum_reg->getOutputPinDynamic("Q"), getOutputPin<32>("SUM_OUT"));
    auto x_reg = l.reg("X", 8, clk, rst, l.bit(true));
    l.wire(getInputPin<8>("X_IN"), x_reg->getInputPinDynamic("D"));
    l.wire(x_reg->getOutputPinDynamic("Q"), getOutputPin<8>("X_OUT"));
    l.finish();
}

Requantizer::Requantizer(std::string name) : IOComponent(std::move(name), requantizerPins) {}

void Requantizer::buildInternals(ComponentBuilder& b) {
    Logic l(b);
    const auto acc = l.split(getInputPin<32>("ACC"));
    const auto scale = l.split(getInputPin<16>("SCALE"));
    const auto shift = l.split(getInputPin<5>("SHIFT"));
    // int32 (signed) × uint16 → 48-bit product.
    const auto product = multiplyBits(b, l, acc, scale, acc.back(), nullptr);
    // Rounding constant 2^(SHIFT-1): bit k is set when SHIFT = k + 1.
    auto decode = b.add(circuit::families::decoder(5), "ROUND_SELECT");
    l.wire(getInputPin<5>("SHIFT"), decode->getInputPinDynamic("ADDR"));
    l.wire(l.bit(true), decode->getInputPin("ENABLE"));
    V round;
    for (size_t k = 0; k < 48; ++k) round.push_back(k < 31 ? decode->getOutputPin("OUT" + std::to_string(k + 1)) : l.bit(false));
    auto value = addRows(b, l, product, round, "ROUND_ADD_");
    // Arithmetic shift right by SHIFT, one stage per SHIFT bit, in 16-bit word muxes.
    const auto sign = value.back();
    for (size_t s = 0; s < 5; ++s) {
        const size_t distance = size_t{1} << s;
        V shifted;
        for (size_t i = 0; i < 48; ++i) shifted.push_back(i + distance < 48 ? value[i + distance] : sign);
        V next;
        for (size_t first = 0; first < 48; first += 16) {
            const V low(value.begin() + first, value.begin() + first + 16);
            const V high(shifted.begin() + first, shifted.begin() + first + 16);
            const auto part = l.mux({shift[s], low, high});
            next.insert(next.end(), part.begin(), part.end());
        }
        value = std::move(next);
    }
    // Saturate to int8: the value fits when bits 7..47 are all equal.
    const V upper(value.begin() + 7, value.end());
    const auto fits = l.lor({l.all(upper), l.inv(l.any(upper))});
    const auto negative = value.back();
    const auto zero = l.land({getInputPin("RELU"), negative});
    const auto keep = l.inv(zero);
    V out;
    for (size_t i = 0; i < 8; ++i) {
        const auto saturated = i < 7 ? l.inv(negative) : negative;    // 0x7F or 0x80
        const auto chosen = l.lor({l.land({fits, value[i]}), l.land({l.inv(fits), saturated})});
        out.push_back(l.land({chosen, keep}));
    }
    l.drive(out, getOutputPin<8>("OUT"));
    l.finish();
}

SystolicArray::SystolicArray(std::string name, size_t size)
    : IOComponent(std::move(name), [size](IOComponent* p) { arrayPins(p, size); }),
      size_(size), type_("SystolicArray<" + std::to_string(size) + ">") {}

void SystolicArray::buildInternals(ComponentBuilder& b) {
    Logic l(b);
    const size_t n = size_;
    const auto clk = getInputPin("CLK"), rst = getInputPin("RST"), shift = getInputPin("SHIFT");
    const auto one = l.bit(true);
    const auto name = [](const char* kind, size_t a, size_t c) { return std::string(kind) + "_" + std::to_string(a) + "_" + std::to_string(c); };
    // Row i's input waits i edges, so it meets the sums coming down from row i-1.
    std::vector<P> row_input(n);
    for (size_t i = 0; i < n; ++i) {
        P x = getInputPin<8>("X_IN_" + std::to_string(i));
        for (size_t k = 0; k < i; ++k) {
            auto reg = l.reg(name("SKEW", i, k), 8, clk, rst, one);
            l.wire(x, reg->getInputPinDynamic("D"));
            x = reg->getOutputPinDynamic("Q");
        }
        row_input[i] = x;
    }
    std::vector<std::vector<std::shared_ptr<IOComponent>>> cells(n, std::vector<std::shared_ptr<IOComponent>>(n));
    const auto zero = l.constantWord(32, 0);
    for (size_t i = 0; i < n; ++i)
        for (size_t j = 0; j < n; ++j) {
            auto cell = b.add(circuit::families::MacCell, name("CELL", i, j));
            cells[i][j] = cell;
            l.wire(clk, cell->getInputPin("CLK"));
            l.wire(rst, cell->getInputPin("RST"));
            l.wire(shift, cell->getInputPin("SHIFT"));
            l.wire(i ? cells[i - 1][j]->getOutputPin<8>("W_OUT") : getInputPin<8>("W_IN_" + std::to_string(j)), cell->getInputPin<8>("W_IN"));
            l.wire(j ? cells[i][j - 1]->getOutputPin<8>("X_OUT") : row_input[i], cell->getInputPin<8>("X_IN"));
            l.wire(i ? cells[i - 1][j]->getOutputPin<32>("SUM_OUT") : zero, cell->getInputPin<32>("SUM_IN"));
        }
    // Column j finishes j edges after column 0; it waits N-1-j more so all
    // columns of one input vector leave together.
    for (size_t j = 0; j < n; ++j) {
        P y = cells[n - 1][j]->getOutputPin<32>("SUM_OUT");
        for (size_t k = 0; k + 1 + j < n; ++k) {
            auto reg = l.reg(name("DESKEW", j, k), 32, clk, rst, one);
            l.wire(y, reg->getInputPinDynamic("D"));
            y = reg->getOutputPinDynamic("Q");
        }
        l.wire(y, getOutputPin<32>("Y_OUT_" + std::to_string(j)));
    }
    l.finish();
}

// Words in the compact forms: a value and whether every bit is known. The
// parts' contract domain is known values; the one unknown case real programs
// reach is kept exact: a product with a known-zero operand is 0 even when the
// other operand is unknown (an unwritten padding byte meeting a zero weight),
// as the gates give. Any other unknown makes the whole result unknown.
struct Word { int64_t value = 0; bool known = true; };
Word readWord(IOComponent& part, const std::string& pin, bool is_signed) {
    const auto bits = part.getInputPinDynamic(pin)->getValueAsVector();
    Word w;
    uint64_t v = 0;
    for (size_t i = 0; i < bits.size(); ++i) {
        if (bits[i] == LogicValue::HIGH) v |= uint64_t{1} << i;
        else if (bits[i] != LogicValue::LOW) w.known = false;
    }
    const size_t n = bits.size();
    w.value = is_signed && n < 64 && (v >> (n - 1)) & 1 ? int64_t(v) - (int64_t{1} << n) : int64_t(v);
    return w;
}
std::vector<LogicValue> bitsOf(const Word& w, size_t width) {
    std::vector<LogicValue> bits(width, LogicValue::UNKNOWN);
    if (!w.known) return bits;
    for (size_t i = 0; i < width; ++i) bits[i] = (uint64_t(w.value) >> i) & 1 ? LogicValue::HIGH : LogicValue::LOW;
    return bits;
}
Word product(const Word& x, const Word& w) {
    if (x.known && w.known) return {x.value * w.value, true};
    if ((x.known && x.value == 0) || (w.known && w.value == 0)) return {0, true};
    return {0, false};
}
Word add32(const Word& a, const Word& b) {
    if (!a.known || !b.known) return {0, false};
    return {int64_t(int32_t(uint32_t(a.value) + uint32_t(b.value))), true};
}

// The compact requantizer: the reference rule on known inputs, all unknown otherwise.
class RequantizerBehavioral final : public BasicComponent {
public:
    explicit RequantizerBehavioral(std::string name) : BasicComponent(std::move(name), 1, requantizerPins) {}
    const char* getTypeName() const override { return Requantizer::TypeName; }
    void evaluate(SimulationTime time, Simulator& sim) override {
        const auto acc = readWord(*this, "ACC", true), scale = readWord(*this, "SCALE", false), shift = readWord(*this, "SHIFT", false);
        const auto relu = getInputValue("RELU");
        Word out{0, acc.known && scale.known && shift.known && (relu == LogicValue::LOW || relu == LogicValue::HIGH)};
        if (out.known) {
            __int128 v = __int128(acc.value) * scale.value;
            if (shift.value) v += __int128(1) << (shift.value - 1);
            v >>= shift.value;
            v = v > 127 ? 127 : v < -128 ? -128 : v;
            if (relu == LogicValue::HIGH && v < 0) v = 0;
            out.value = int64_t(v);
        }
        _updateOutputWireDynamic(sim, "OUT", bitsOf(out, 8), time);
    }
};

// The compact array: every register of the gate-level array (weights, cell
// sums and inputs, skew and de-skew registers) as host words, updated at a
// clean rising clock edge from the values present at the edge, as the gates
// do. Reset clears them, as it clears every register of the gates.
class SystolicArrayBehavioral final : public BasicComponent {
public:
    SystolicArrayBehavioral(std::string name, size_t n)
        : BasicComponent(std::move(name), 1, [n](IOComponent* p) { arrayPins(p, n); }), n_(n),
          type_("SystolicArray<" + std::to_string(n) + ">"),
          weight_(n * n), x_(n * n), sum_(n * n), skew_(n), deskew_(n) {
        for (size_t i = 0; i < n; ++i) skew_[i].resize(i);
        for (size_t j = 0; j < n; ++j) deskew_[j].resize(n - 1 - j);
    }
    const char* getTypeName() const override { return type_.c_str(); }
    void evaluate(SimulationTime time, Simulator& sim) override {
        const auto clock = getInputValue("CLK"), reset = getInputValue("RST");
        if (reset == LogicValue::HIGH) clear(Word{0, true});
        else if (reset != LogicValue::LOW) clearUnknownNonZero();
        else if (previous_clock_ == LogicValue::LOW && clock == LogicValue::HIGH) step();
        previous_clock_ = clock;
        for (size_t j = 0; j < n_; ++j) {
            const auto& y = deskew_[j].empty() ? sum_[(n_ - 1) * n_ + j] : deskew_[j].back();
            _updateOutputWireDynamic(sim, "Y_OUT_" + std::to_string(j), bitsOf(y, 32), time);
        }
    }
private:
    size_t at(size_t i, size_t j) const { return i * n_ + j; }
    void clear(Word w) {
        for (auto* v : {&weight_, &x_, &sum_}) std::fill(v->begin(), v->end(), w);
        for (auto& c : skew_) std::fill(c.begin(), c.end(), w);
        for (auto& c : deskew_) std::fill(c.begin(), c.end(), w);
    }
    void clearUnknownNonZero() {
        auto f = [](Word& w) { if (!(w.known && w.value == 0)) w = {0, false}; };
        for (auto* v : {&weight_, &x_, &sum_}) for (auto& w : *v) f(w);
        for (auto& c : skew_) for (auto& w : c) f(w);
        for (auto& c : deskew_) for (auto& w : c) f(w);
    }
    void step() {
        const auto shift = getInputValue("SHIFT");
        std::vector<Word> in(n_), w_in(n_);
        for (size_t i = 0; i < n_; ++i) in[i] = readWord(*this, "X_IN_" + std::to_string(i), true);
        for (size_t j = 0; j < n_; ++j) w_in[j] = readWord(*this, "W_IN_" + std::to_string(j), true);
        auto weight = weight_, x = x_, sum = sum_;
        for (size_t i = 0; i < n_; ++i)
            for (size_t j = 0; j < n_; ++j) {
                const Word row_input = i == 0 ? in[0] : skew_[i].back();
                const Word xin = j == 0 ? row_input : x_[at(i, j - 1)];
                const Word sin = i == 0 ? Word{0, true} : sum_[at(i - 1, j)];
                const Word win = i == 0 ? w_in[j] : weight_[at(i - 1, j)];
                sum[at(i, j)] = add32(sin, product(xin, weight_[at(i, j)]));
                x[at(i, j)] = xin;
                if (shift == LogicValue::HIGH) weight[at(i, j)] = win;
                else if (shift != LogicValue::LOW && !(win.known && weight_[at(i, j)].known && win.value == weight_[at(i, j)].value))
                    weight[at(i, j)] = {0, false};
            }
        for (size_t i = 1; i < n_; ++i) {
            for (size_t k = skew_[i].size() - 1; k > 0; --k) skew_[i][k] = skew_[i][k - 1];
            skew_[i][0] = in[i];
        }
        for (size_t j = 0; j < n_; ++j) {
            if (deskew_[j].empty()) continue;
            for (size_t k = deskew_[j].size() - 1; k > 0; --k) deskew_[j][k] = deskew_[j][k - 1];
            deskew_[j][0] = sum_[at(n_ - 1, j)];
        }
        weight_ = std::move(weight); x_ = std::move(x); sum_ = std::move(sum);
    }
    size_t n_;
    std::string type_;
    std::vector<Word> weight_, x_, sum_;
    std::vector<std::vector<Word>> skew_, deskew_;
    LogicValue previous_clock_ = LogicValue::UNKNOWN;
};

// The array's timing, composed from the 2 × 2 gate-level array: every larger
// array has the same paths (row 0 into a cell, other rows into a skew
// register, weights into a cell, outputs from a cell or a de-skew register).
PartDelays arrayDelays(size_t n) {
    const auto& two = gateListDelays("accelerator.systolic-array.2");
    PartDelays d;
    d.evidence = "composed from the gate-level SystolicArray<2> (" + two.evidence + ")";
    d.clock = two.clock;
    d.clock_outputs = two.clock_outputs;
    d.internal_period = two.internal_period;
    d.fixed_inputs = two.fixed_inputs;
    DelayRange out{~SimulationTime{0}, 0};
    for (const auto& [name, range] : two.launches) { out.minimum = std::min(out.minimum, range.minimum); out.maximum = std::max(out.maximum, range.maximum); }
    if (two.launches.empty()) out = {0, 0};
    for (size_t j = 0; j < n; ++j) d.launches["Y_OUT_" + std::to_string(j)] = out;
    const auto setup = [&](const std::string& name) { const auto it = two.setup.find(name); return it == two.setup.end() ? SimulationTime{0} : it->second; };
    const auto weight_setup = std::max(setup("W_IN_0"), setup("W_IN_1"));
    for (size_t i = 0; i < n; ++i) {
        d.setup["X_IN_" + std::to_string(i)] = i == 0 ? setup("X_IN_0") : setup("X_IN_1");
        d.setup["W_IN_" + std::to_string(i)] = weight_setup;
    }
    if (two.setup.contains("SHIFT")) d.setup["SHIFT"] = setup("SHIFT");
    if (two.setup.contains("RST")) d.setup["RST"] = setup("RST");
    return d;
}

void registerMatrixParts(ComponentCatalog& c) {
    using circuit::registration::registerStructural;
    using circuit::registration::registerBehavioral;
    registerStructural(c, circuit::families::MacCell, "MacCellTest", "clock-boundary");
    registerBehavioral(c, circuit::families::MacCell, "MacCellTest", "MacCellScenarioTest");
    registerStructural(c, circuit::families::Requantizer, "RequantizerTest");
    registerBehavioral(c, circuit::families::Requantizer, "RequantizerTest", "RequantizerScenarioTest", {}, "stable-after-settle", "known-binary");
    for (size_t n : {2u, 4u, 8u, 16u}) {
        registerStructural(c, circuit::families::systolicArray(n), "SystolicArrayTest", "clock-boundary");
        registerBehavioral(c, circuit::families::systolicArray(n), "SystolicArrayTest", "SystolicArrayWidth2ScenarioTest",
                           {}, "clock-boundary", "known-binary");
    }
}
} // namespace circuit::accelerator

namespace circuit::families {
const ComponentFamily MacCell{"accelerator.mac-cell", "MacCell", cellPins,
    [](const std::string& n, const std::shared_ptr<BuildContext>& c) { return Component::createWithContext<accelerator::MacCell>(c, n); },
    [](const std::string& n, const std::shared_ptr<BuildContext>& c) { return circuit::detail::createGateListPart(MacCell, n, c); }};
const ComponentFamily Requantizer{"accelerator.requantize", "Requantizer", requantizerPins,
    [](const std::string& n, const std::shared_ptr<BuildContext>& c) { return Component::createWithContext<accelerator::Requantizer>(c, n); },
    [](const std::string& n, const std::shared_ptr<BuildContext>& c) {
        auto part = Component::createWithContext<accelerator::RequantizerBehavioral>(c, n);
        part->setDelays(gateListDelays("accelerator.requantize"));
        return part; }};
namespace {
template<size_t N> const ComponentFamily& arrayFamily() {
    static const std::string id = "accelerator.systolic-array." + std::to_string(N);
    static const std::string type = "SystolicArray<" + std::to_string(N) + ">";
    static const ComponentFamily f{id, type, [](IOComponent* p) { arrayPins(p, N); },
        [](const std::string& n, const std::shared_ptr<BuildContext>& c) { return Component::createWithContext<accelerator::SystolicArray>(c, n, N); },
        [](const std::string& n, const std::shared_ptr<BuildContext>& c) {
            auto part = Component::createWithContext<accelerator::SystolicArrayBehavioral>(c, n, N);
            part->setDelays(accelerator::arrayDelays(N));
            return part; }};
    return f;
}
}
const ComponentFamily& systolicArray(size_t n) {
    switch (n) {
    case 2: return arrayFamily<2>();
    case 4: return arrayFamily<4>();
    case 8: return arrayFamily<8>();
    case 16: return arrayFamily<16>();
    }
    throw std::invalid_argument("Invalid systolic array size");
}
}
