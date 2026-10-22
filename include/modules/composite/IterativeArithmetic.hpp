#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"

namespace circuit::arithmetic {
enum class Operation { Multiply, Divide, MulDiv };
class IterativeUnit : public IOComponent {
    size_t width_;Operation operation_;std::string type_;
public:
    IterativeUnit(std::string,size_t width,Operation);
    const char* getTypeName() const override {return type_.c_str();}
    void buildInternals(ComponentBuilder&) override;
};
}
namespace circuit::families {
const ComponentFamily& multiplier(size_t width);
const ComponentFamily& divider(size_t width);
const ComponentFamily& mulDivUnit(size_t width);
}
template<size_t W> class Multiplier : public circuit::arithmetic::IterativeUnit {
    static_assert(W==4 || W==8 || W==16 || W==32);
public:explicit Multiplier(std::string n):IterativeUnit(std::move(n),W,circuit::arithmetic::Operation::Multiply){}
};
template<size_t W> class Divider : public circuit::arithmetic::IterativeUnit {
    static_assert(W==4 || W==8 || W==16 || W==32);
public:explicit Divider(std::string n):IterativeUnit(std::move(n),W,circuit::arithmetic::Operation::Divide){}
};
template<size_t W> class MulDivUnit : public circuit::arithmetic::IterativeUnit {
    static_assert(W==4 || W==8 || W==16 || W==32);
public:explicit MulDivUnit(std::string n):IterativeUnit(std::move(n),W,circuit::arithmetic::Operation::MulDiv){}
};
