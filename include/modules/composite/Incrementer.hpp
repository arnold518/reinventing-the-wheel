#pragma once
#include "components/IOComponent.hpp"
#include <vector>
#include <cstdint>

namespace circuit::word {
class WordIncrementer : public IOComponent {
    size_t width_;uint64_t increment_;std::string type_;
public:
    WordIncrementer(std::string,size_t width,uint64_t increment);
    const char* getTypeName()const override{return type_.c_str();}
    void buildInternals(ComponentBuilder&)override;
};
// Gate-equivalent settled values for parent components that hold a counter/PC.
std::vector<LogicValue> incrementWord(const std::vector<LogicValue>&,uint64_t increment);
}
template<size_t W,uint64_t Step=1>class Incrementer : public circuit::word::WordIncrementer {
    static_assert(W==8 || W==16 || W==32);
    static_assert(Step<(uint64_t{1}<<W));
public:explicit Incrementer(std::string name):WordIncrementer(std::move(name),W,Step){}
};
