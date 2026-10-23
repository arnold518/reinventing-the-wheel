#pragma once
#include "modules/composite/WordSliceBuilder.hpp"
#include "modules/composite/WordRecipes.hpp"
#include "modules/basic/WordMux.hpp"
#include <algorithm>
namespace circuit::word {
class WordBuilder : public circuit::detail::WordSliceBuilder {
    size_t width_;
    template<size_t W> Signal joinSized(const std::string& name,const std::vector<Signal>& v) {
        std::array<Signal,W> a; std::copy(v.begin(),v.end(),a.begin()); return join<W>(name,a);
    }
public:
    using Bits = std::vector<Signal>;
    WordBuilder(IOComponent& root,ComponentBuilder& b,size_t width)
        : WordSliceBuilder(root,b),width_(width) {}
    Bits splitWord(const std::string& name,Signal src) {
        if(width_==8){auto a=split<8>(name,src);return {a.begin(),a.end()};}
        if(width_==16){auto a=split<16>(name,src);return {a.begin(),a.end()};}
        auto a=split<32>(name,src);return {a.begin(),a.end()};
    }
    Signal joinWord(const std::string& name,const Bits& bits) {
        if(bits.size()!=width_)throw std::invalid_argument("Word join width mismatch");
        if(width_==8)return joinSized<8>(name,bits);
        if(width_==16)return joinSized<16>(name,bits);
        return joinSized<32>(name,bits);
    }
    Signal constantWord(const std::string& name,uint64_t value) {
        if(width_==8)return constant<8>(name,value);
        if(width_==16)return constant<16>(name,value);
        return constant<32>(name,value);
    }
    Signal maskBus(const std::string& name,Signal bus,Signal enable) {
        auto bits=splitWord(name+"_SPLIT",bus);
        for(size_t i=0;i<width_;++i)bits[i]=binary<ANDGate>(name+"_BIT_"+std::to_string(i),bits[i],enable);
        return joinWord(name+"_JOIN",bits);
    }
    template<size_t Inputs>
    Signal selectBus(const std::string& name,const std::array<Signal,Inputs>& inputs,Signal op) {
        auto c=add(circuit::word::mux(Inputs,width_),name);
        connect(op,c->getInputPinDynamic("SEL"));
        for(size_t i=0;i<Inputs;++i)connect(inputs[i],c->getInputPinDynamic(Inputs==2?(i?"B":"A"):"IN"+std::to_string(i)));
        return c->getOutputPinDynamic("OUT");
    }
    std::shared_ptr<IOComponent> addWord(Unit u,const std::string& name) { return add(family(u,width_),name); }
};
}
