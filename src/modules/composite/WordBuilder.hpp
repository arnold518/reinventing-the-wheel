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
    // Split or join a word of any width (splitters/joiners at 4, 8, 16 and 32
    // bits, a zero-delay Rewire otherwise).
    Bits splitBits(const std::string& name,Signal src) {
        switch(src->getWidth()){
        case 4:{auto a=split<4>(name,src);return {a.begin(),a.end()};}
        case 8:{auto a=split<8>(name,src);return {a.begin(),a.end()};}
        case 16:{auto a=split<16>(name,src);return {a.begin(),a.end()};}
        case 32:{auto a=split<32>(name,src);return {a.begin(),a.end()};}
        default:{
            // Other widths: one zero-delay Rewire (wiring only).
            const size_t width=src->getWidth();
            std::vector<Rewire::WireSpec> outs; std::vector<Rewire::BitMap> map;
            for(size_t i=0;i<width;++i){outs.emplace_back("OUT_"+std::to_string(i),1);map.emplace_back("IN",i,"OUT_"+std::to_string(i),0);}
            auto r=add<Rewire>(name,std::vector<Rewire::WireSpec>{{"IN",width}},outs,map);
            connect(src,r->getInputPinDynamic("IN"));
            Bits bits; for(size_t i=0;i<width;++i)bits.push_back(r->getOutputPinDynamic("OUT_"+std::to_string(i)));
            return bits;
        }
        }
    }
    Signal joinBits(const std::string& name,const Bits& bits) {
        switch(bits.size()){
        case 4:return joinSized<4>(name,bits);
        case 8:return joinSized<8>(name,bits);
        case 16:return joinSized<16>(name,bits);
        case 32:return joinSized<32>(name,bits);
        default:{
            std::vector<Rewire::WireSpec> ins; std::vector<Rewire::BitMap> map;
            for(size_t i=0;i<bits.size();++i){ins.emplace_back("IN_"+std::to_string(i),1);map.emplace_back("IN_"+std::to_string(i),0,"OUT",i);}
            auto r=add<Rewire>(name,ins,std::vector<Rewire::WireSpec>{{"OUT",bits.size()}},map);
            for(size_t i=0;i<bits.size();++i)connect(bits[i],r->getInputPinDynamic("IN_"+std::to_string(i)));
            return r->getOutputPinDynamic("OUT");
        }
        }
    }
    Bits splitWord(const std::string& name,Signal src) {
        if(src->getWidth()!=width_)throw std::invalid_argument("Word split width mismatch");
        return splitBits(name,src);
    }
    Signal joinWord(const std::string& name,const Bits& bits) {
        if(bits.size()!=width_)throw std::invalid_argument("Word join width mismatch");
        return joinBits(name,bits);
    }
    Signal constantWord(const std::string& name,uint64_t value) {
        if(width_==8)return constant<8>(name,value);
        if(width_==16)return constant<16>(name,value);
        return constant<32>(name,value);
    }
    // AND every bit of a word with one enable: one word gate, the enable
    // spread across the word by wiring.
    Signal maskBus(const std::string& name,Signal bus,Signal enable) {
        return wordGate(circuit::word::GateKind::And,name,bus,broadcast(name+"_ENABLE",enable,width_));
    }
    // reduce() on whole words: the same balanced pairing and node names, one
    // word gate per node.
    Signal reduceWords(const std::string& name,std::vector<Signal> words,circuit::word::GateKind kind) {
        for(size_t level=0;words.size()>1;++level){
            std::vector<Signal> next;
            for(size_t i=0;i<words.size();i+=2){
                if(i+1==words.size())next.push_back(words[i]);
                else next.push_back(wordGate(kind,name+"_"+std::to_string(level)+"_"+std::to_string(i/2),words[i],words[i+1]));
            }
            words=std::move(next);
        }
        return words.front();
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
