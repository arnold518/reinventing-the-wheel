#pragma once
// Construction helpers only. Every operation below builds existing gates,
// rewires, adders or register families; this is not a behavioral evaluator.
#include "components/StructuralNetBuilder.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/basic/WordGate.hpp"
#include "modules/basic/WordMux.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/storage/Register.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Rewire.hpp"
#include <array>
#include <map>
#include <utility>
namespace circuit::structural {
using P = std::shared_ptr<PinBase>;
using V = std::vector<P>;
class Logic {
public:
    // No one-bit adapters and no split/join round trips; one bridge only where
    // a parent input feeds a parent output. (The full split/join "Legacy"
    // construction was retired with the Device I/O redesign.)
    enum class Adapters { Minimal };
private:
    ComponentBuilder& b;
    circuit::detail::StructuralWiring w;
    size_t n = 0;
    Adapters adapters_;
    std::array<P, 2> bit_constants_;
    // Each split source (by pin ID) with its bit pins, so a later join of
    // exactly those bits returns the source word instead of rebuilding it.
    std::map<std::string, std::pair<P, V>> splits_;
    bool minimal() const { return true; }
    static bool wordAdder(size_t width) { return width == 8 || width == 16 || width == 32; }
    // The word a bit vector came from, without building a join when it is one
    // whole split; otherwise one named join Rewire.
    P wordOf(const V& bits, const std::string& name) { return w.joinBits(name, bits); }
    // One NOTGate<W> instead of W inverters: the same gates inside, between
    // zero-delay Rewires, so timing is unchanged. It uses up the W NOT_n
    // numbers the separate inverters had and is named after the first.
    P invertedWord(const V& bits) {
        std::string first;
        for (size_t i = 0; i < bits.size(); ++i) { auto name = id("NOT"); if (i == 0) first = name; }
        auto inverter = b.add(circuit::families::wordGate(circuit::word::GateKind::Not, bits.size()), "WORD_" + first);
        w.connect(wordOf(bits, "WORD_" + first + "_JOIN"), inverter->getInputPinDynamic("IN"));
        return inverter->getOutputPinDynamic("OUT");
    }
    // The prefix adder of add() on words, with add()'s names: ADD<W>, then a
    // JOIN_n number for each operand.
    std::pair<V,P> addWords(const V& a, P inverted_b, P carry) {
        const auto width = a.size();
        auto p = b.add(circuit::families::parallelPrefixAdder(width), id("ADD" + std::to_string(width)));
        w.connect(join(a), p->getInputPinDynamic("A"));
        (void)id("JOIN"); w.connect(inverted_b, p->getInputPinDynamic("B"));
        w.connect(carry, p->getInputPin("C_IN"));
        return {split(p->getOutputPinDynamic("OUT")), p->getOutputPin("C_OUT")};
    }
    // A component input cannot drive its own output directly; one zero-delay
    // identity Rewire bridges it.
    P bridge(const P& source, const P& sink) {
        const auto width = source->getWidth();
        std::vector<Rewire::BitMap> map;
        for (size_t i = 0; i < width; ++i) map.emplace_back("IN", i, "OUT", i);
        auto pass = b.addNewComponent<Rewire>("PASS_" + sink->getName(),
            std::vector<Rewire::WireSpec>{{"IN", width}}, std::vector<Rewire::WireSpec>{{"OUT", width}}, map);
        w.connect(source, pass->getInputPinDynamic("IN"));
        return pass->getOutputPinDynamic("OUT");
    }
    template<size_t Width>
    P wordConstant(uint64_t value) {
        return b.addNewComponent<ConstantValue<Width>>(
            id(value ? "CONSTANT_WORD" : "ZERO_WORD"), value)->getOutputPinDynamic("OUT");
    }
public:
    explicit Logic(ComponentBuilder& builder, Adapters adapters = Adapters::Minimal)
        : b(builder), w(builder), adapters_(adapters) {}
    std::string id(const std::string& s) { return s + "_" + std::to_string(n++); }
    P bit(bool v) {
        auto& source = bit_constants_[v ? 1 : 0];
        if (!source) source = w.constant(id(v ? "ONE" : "ZERO"), v);
        return source;
    }
    // A tied bus is one existing constant primitive, not one source per bit.
    // Keep it a bus until a gate-level consumer actually needs its bits.
    P constantWord(size_t width, uint64_t value = 0) {
        switch (width) {
        case 1: return bit((value & 1U) != 0);
        case 2: return wordConstant<2>(value);
        case 3: return wordConstant<3>(value);
        case 4: return wordConstant<4>(value);
        case 5: return wordConstant<5>(value);
        case 8: return wordConstant<8>(value);
        case 16: return wordConstant<16>(value);
        case 32: return wordConstant<32>(value);
        default: throw std::invalid_argument("Unsupported structural constant width");
        }
    }
    V constant(size_t width, uint64_t value = 0) {
        auto source = constantWord(width, value);
        return width == 1 ? V{source} : split(source);
    }
    P inv(P a) { return w.invert(id("NOT"), a); }
    V inverted(const V& bits) { V r; for(const auto& x:bits) r.push_back(inv(x)); return r; }
    // Aggregate arguments sequence nested construction left to right on both
    // GCC and clang. Gate names/topology must not depend on argument order.
    struct Pair { P a, b; };
    struct Choice { P select, low, high; };
    struct WordPair { V a, b; };
    struct WordChoice { P select; V low, high; };
    struct AddInputs { V a, b; P carry; };
    P land(Pair pins) { return w.both(id("AND"),pins.a,pins.b); }
    P lor(Pair pins) { return w.either(id("OR"),pins.a,pins.b); }
    P lx(Pair pins) { return w.binary<XORGate>(id("XOR"),pins.a,pins.b); }
    P all(V v) { return w.reduce(id("ALL"),std::move(v),true); }
    P any(V v) { return w.reduce(id("ANY"),std::move(v),false); }
    P choose(Choice pins) {
        return lor({land({inv(pins.select),pins.low}),land({pins.select,pins.high})});
    }
    // Every bit of a word ANDed with one enable bit. Minimal, at a word-gate
    // width: one ANDGate<W> with the enable spread by a Rewire, the same gates
    // and depth, using up the AND_n numbers the separate gates had.
    V mask(const V& bits, P enable) {
        if (minimal() && circuit::word::gateWidth(bits.size())) {
            std::string first;
            for (size_t i = 0; i < bits.size(); ++i) { auto name = id("AND"); if (i == 0) first = name; }
            auto gate = b.add(circuit::families::wordGate(circuit::word::GateKind::And, bits.size()), "WORD_" + first);
            w.connect(wordOf(bits, "WORD_" + first + "_JOIN"), gate->getInputPinDynamic("A"));
            std::vector<Rewire::BitMap> map;
            for (size_t i = 0; i < bits.size(); ++i) map.emplace_back("IN", 0, "OUT", i);
            auto spread = b.addNewComponent<Rewire>("WORD_" + first + "_ENABLE",
                std::vector<Rewire::WireSpec>{{"IN", 1}}, std::vector<Rewire::WireSpec>{{"OUT", bits.size()}}, map);
            w.connect(enable, spread->getInputPinDynamic("IN"));
            w.connect(spread->getOutputPinDynamic("OUT"), gate->getInputPinDynamic("B"));
            return split(gate->getOutputPinDynamic("OUT"));
        }
        V r; for (const auto& x : bits) r.push_back(land({x, enable})); return r;
    }
    V mux(WordChoice pins) {
        const auto& zero=pins.low;const auto& one=pins.high;auto sel=pins.select;
        if(zero.size()!=one.size()) throw std::invalid_argument("Mux width mismatch");
        const auto width=zero.size();
        if(minimal() && (width==4 || width==8 || width==16 || width==32)) {
            // One Mux<2,W>: the same per-bit AND/AND/OR with one shared select
            // inverter, as below. It uses up the NOT/AND/AND/OR numbers.
            std::string first;
            for(size_t i=0;i<width;++i) for(const char* kind:{"NOT","AND","AND","OR"}) {
                auto name=id(kind); if(first.empty()) first=name;
            }
            const auto name="WORD_MUX_"+first.substr(4);
            auto m=b.add(circuit::word::mux(2,width),name);
            w.connect(wordOf(zero,name+"_A"),m->getInputPinDynamic("A"));
            w.connect(wordOf(one,name+"_B"),m->getInputPinDynamic("B"));
            w.connect(sel,m->getInputPinDynamic("SEL"));
            return split(m->getOutputPinDynamic("OUT"));
        }
        // Minimal shares one select inverter; every bit still uses up its
        // NOT/AND/AND/OR names so later child names do not shift.
        P inverted;
        V r;
        for(size_t i=0;i<zero.size();++i) {
            const auto not_name=id("NOT");
            if(!inverted || !minimal()) inverted=w.invert(not_name,sel);
            auto low=land({inverted,zero[i]});
            auto high=land({sel,one[i]});
            r.push_back(lor({low,high}));
        }
        return r;
    }
    V negate(const V& a) {
        if (minimal() && wordAdder(a.size())) {
            // Same order of names as below: NOT_n x W, the zero word, ONE, ADD.
            auto inverted = invertedWord(a);
            auto zero = constant(a.size());
            auto one = bit(true);
            const auto width = a.size();
            auto p = b.add(circuit::families::parallelPrefixAdder(width), id("ADD" + std::to_string(width)));
            (void)id("JOIN"); w.connect(inverted, p->getInputPinDynamic("A"));
            w.connect(join(zero), p->getInputPinDynamic("B"));
            w.connect(one, p->getInputPin("C_IN"));
            return split(p->getOutputPinDynamic("OUT"));
        }
        V z;for(auto p:a)z.push_back(inv(p));return add({z,constant(a.size()),bit(true)}).first;
    }
    P equal(WordPair pins) {
        const auto& a=pins.a;const auto& c=pins.b;
        V r;for(size_t i=0;i<a.size();++i)r.push_back(inv(lx({a[i],c.at(i)})));return all(r);
    }
    // Skipped adapters still use up their SPLIT_n/JOIN_n numbers, so the
    // names of the remaining children match the legacy construction.
    V split(P src) {
        const auto name=id("SPLIT");
        const auto width=src->getWidth();
        if(width==1) return V{src};
        if(auto found=splits_.find(src->getID()); found!=splits_.end()) return found->second.second;
        auto r=w.splitWord(name,src);
        splits_[src->getID()]={src,r};
        return r;
    }
    P join(const V& bits) {
        const auto name=id("JOIN");
        if(bits.size()==1) return bits.front();
        return w.joinBits(name,bits);
    }
    std::pair<V,P> add(AddInputs pins) {
        const auto& a=pins.a;const auto& c=pins.b;auto carry=pins.carry;
        if(a.size()!=c.size()) throw std::invalid_argument("Adder width mismatch");
        const auto width=a.size();
        if(width==8 || width==16 || width==32) {
            auto p=b.add(circuit::families::parallelPrefixAdder(width),id("ADD"+std::to_string(width)));
            w.connect(join(a),p->getInputPinDynamic("A"));w.connect(join(c),p->getInputPinDynamic("B"));
            w.connect(carry,p->getInputPin("C_IN"));return {split(p->getOutputPinDynamic("OUT")),p->getOutputPin("C_OUT")};
        }
        V r;for(size_t i=0;i<a.size();++i){
            auto p=b.addNewComponent<FullAdder>(id("FA"));
            w.connect(a[i],p->getInputPin("A"));w.connect(c[i],p->getInputPin("B"));w.connect(carry,p->getInputPin("Carry_in"));
            r.push_back(p->getOutputPin("Sum"));carry=p->getOutputPin("Carry_out");
        }return {r,carry};
    }
    std::pair<V,P> sub(WordPair pins) {
        if (minimal() && pins.a.size() == pins.b.size() && wordAdder(pins.b.size())) {
            auto inverted = invertedWord(pins.b);
            auto one = bit(true);
            return addWords(pins.a, inverted, one);
        }
        V z;for(auto p:pins.b)z.push_back(inv(p));return add({pins.a,z,bit(true)});
    }
    V shiftLeft(V a,P low) { a.pop_back();a.insert(a.begin(),low);return a; }
    V shiftRight(V a,P high) { a.erase(a.begin());a.push_back(high);return a; }
    std::shared_ptr<IOComponent> reg(const std::string& name,size_t width,P clk,P rst,P enable) {
        auto p=b.add(circuit::families::registerWord(width),name);
        w.connect(clk,p->getInputPin("CLK"));w.connect(rst,p->getInputPin("RST"));w.connect(enable,p->getInputPin("WE"));return p;
    }
    void drive(const V& v,P sink) { wire(join(v),sink); }
    void wire(P src,P sink) {
        if(src->getType()==PinType::INPUT && sink->getType()==PinType::OUTPUT && src->getOwner()==sink->getOwner())
            src=bridge(src,sink);
        w.connect(src,sink);
    }
    // Minimal: a word is wired straight through (one bridge if it is a parent
    // input); the split/join numbers are still used up, as before.
    void out(P src,P sink) {
        if(minimal()) { (void)id("SPLIT"); (void)id("JOIN"); wire(src,sink); return; }
        drive(split(src),sink);
    }
    // Word-level arithmetic that keeps results as words, so a recipe that only
    // passes them on never builds a split. Same gates as sub()/negate().
    P invertWord(P x) {
        const auto width=x->getWidth();
        if(!minimal() || !circuit::word::gateWidth(width)) return join(inverted(split(x)));
        std::string first;
        for(size_t i=0;i<width;++i){ auto name=id("NOT"); if(i==0) first=name; }
        auto inverter=b.add(circuit::families::wordGate(circuit::word::GateKind::Not,width),"WORD_"+first);
        w.connect(x,inverter->getInputPinDynamic("IN"));
        return inverter->getOutputPinDynamic("OUT");
    }
    P subtractWords(P x, P y) {
        const auto width=x->getWidth();
        if(!minimal() || !wordAdder(width)) return join(sub({split(x),split(y)}).first);
        auto flipped=invertWord(y); auto one=bit(true);
        auto p=b.add(circuit::families::parallelPrefixAdder(width),id("ADD"+std::to_string(width)));
        w.connect(x,p->getInputPinDynamic("A")); w.connect(flipped,p->getInputPinDynamic("B"));
        w.connect(one,p->getInputPin("C_IN"));
        return p->getOutputPinDynamic("OUT");
    }
    // join(add({split(x), y, bit(carry_in)}).first) for a word x: the same
    // adder and names, without splitting x or the sum.
    std::pair<P,P> addToWord(P x, const V& y, bool carry_in) {
        const auto width=y.size();
        if(!minimal() || !wordAdder(width) || x->getWidth()!=width) {
            auto [sum,carry]=add({split(x),y,bit(carry_in)});
            return {join(sum),carry};
        }
        (void)id("SPLIT"); auto carry=bit(carry_in);
        auto p=b.add(circuit::families::parallelPrefixAdder(width),id("ADD"+std::to_string(width)));
        (void)id("JOIN"); w.connect(x,p->getInputPinDynamic("A"));
        w.connect(join(y),p->getInputPinDynamic("B"));
        w.connect(carry,p->getInputPin("C_IN"));
        (void)id("SPLIT"); (void)id("JOIN");
        return {p->getOutputPinDynamic("OUT"),p->getOutputPin("C_OUT")};
    }
    // sub({a, split(y)}) without the split of y: the same names and gates.
    std::pair<V,P> subtractWord(const V& a, P y) {
        if(!minimal() || !wordAdder(a.size()) || y->getWidth()!=a.size()) return sub({a,split(y)});
        (void)id("SPLIT");
        auto inverted=invertWord(y); auto one=bit(true);
        return addWords(a,inverted,one);
    }
    P negateWord(P x) {
        const auto width=x->getWidth();
        if(!minimal() || !wordAdder(width)) return join(negate(split(x)));
        auto flipped=invertWord(x); auto zero=constantWord(width); auto one=bit(true);
        auto p=b.add(circuit::families::parallelPrefixAdder(width),id("ADD"+std::to_string(width)));
        w.connect(flipped,p->getInputPinDynamic("A")); w.connect(zero,p->getInputPinDynamic("B"));
        w.connect(one,p->getInputPin("C_IN"));
        return p->getOutputPinDynamic("OUT");
    }
    void finish() { w.finish(); }
};
}
