#pragma once
// Construction helpers only. Every operation below builds existing gates,
// rewires, adders or register families; this is not a behavioral evaluator.
#include "components/StructuralNetBuilder.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/composite/FullAdder.hpp"
#include "modules/storage/Register.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Rewire.hpp"
#include <array>
namespace circuit::structural {
using P = std::shared_ptr<PinBase>;
using V = std::vector<P>;
class Logic {
    ComponentBuilder& b;
    circuit::detail::StructuralWiring w;
    size_t n = 0;
public:
    explicit Logic(ComponentBuilder& builder) : b(builder), w(builder) {}
    std::string id(const std::string& s) { return s + "_" + std::to_string(n++); }
    P bit(bool v) { return w.constant(id(v ? "ONE" : "ZERO"), v); }
    V constant(size_t width, uint64_t value = 0) {
        V r; for(size_t i=0;i<width;++i) r.push_back(bit((value >> i)&1)); return r;
    }
    P inv(P a) { return w.invert(id("NOT"), a); }
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
    V mux(WordChoice pins) {
        const auto& zero=pins.low;const auto& one=pins.high;auto sel=pins.select;
        if(zero.size()!=one.size()) throw std::invalid_argument("Mux width mismatch");
        V r; for(size_t i=0;i<zero.size();++i) r.push_back(choose({sel,zero[i],one[i]})); return r;
    }
    V negate(const V& a) { V z;for(auto p:a)z.push_back(inv(p));return add({z,constant(a.size()),bit(true)}).first; }
    P equal(WordPair pins) {
        const auto& a=pins.a;const auto& c=pins.b;
        V r;for(size_t i=0;i<a.size();++i)r.push_back(inv(lx({a[i],c.at(i)})));return all(r);
    }
    V split(P src) {
        const auto width=src->getWidth();
        std::vector<Rewire::WireSpec> outs;std::vector<Rewire::BitMap> map;
        for(size_t i=0;i<width;++i){outs.emplace_back("B"+std::to_string(i),1);map.emplace_back("IN",i,"B"+std::to_string(i),0);}
        auto p=b.addNewComponent<Rewire>(id("SPLIT"),std::vector<Rewire::WireSpec>{{"IN",width}},outs,map);
        w.connect(src,p->getInputPinDynamic("IN"));V r;
        for(size_t i=0;i<width;++i)r.push_back(p->getOutputPin("B"+std::to_string(i)));return r;
    }
    P join(const V& bits) {
        std::vector<Rewire::WireSpec> ins;std::vector<Rewire::BitMap> map;
        for(size_t i=0;i<bits.size();++i){ins.emplace_back("B"+std::to_string(i),1);map.emplace_back("B"+std::to_string(i),0,"OUT",i);}
        auto p=b.addNewComponent<Rewire>(id("JOIN"),ins,std::vector<Rewire::WireSpec>{{"OUT",bits.size()}},map);
        for(size_t i=0;i<bits.size();++i)w.connect(bits[i],p->getInputPin("B"+std::to_string(i)));
        return p->getOutputPinDynamic("OUT");
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
        V z;for(auto p:pins.b)z.push_back(inv(p));return add({pins.a,z,bit(true)});
    }
    V shiftLeft(V a,P low) { a.pop_back();a.insert(a.begin(),low);return a; }
    V shiftRight(V a,P high) { a.erase(a.begin());a.push_back(high);return a; }
    std::shared_ptr<IOComponent> reg(const std::string& name,size_t width,P clk,P rst,P enable) {
        auto p=b.add(circuit::families::registerWord(width),name);
        w.connect(clk,p->getInputPin("CLK"));w.connect(rst,p->getInputPin("RST"));w.connect(enable,p->getInputPin("WE"));return p;
    }
    void drive(const V& v,P sink) { w.connect(join(v),sink); }
    void wire(P src,P sink) { w.connect(src,sink); }
    void out(P src,P sink) { drive(split(src),sink); }
    void finish() { w.finish(); }
};
}
