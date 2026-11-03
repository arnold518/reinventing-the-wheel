#pragma once

// Private wiring-only groups, like the groups in RV32IExecuteStage. They add
// circuit hierarchy, not behavioral replacements or new catalog contracts.
#include "components/StructuralLogic.hpp"
#include "modules/basic/WordMux.hpp"

namespace rv32im::detail {
using circuit::structural::Logic;
using circuit::structural::P;
using circuit::structural::V;

inline P selectWord(ComponentBuilder& b, Logic& l, const std::string& name,
                    P select, P low, P high) {
    auto mux=b.add(circuit::word::mux(2,low->getWidth()),name);
    l.wire(select,mux->getInputPin("SEL"));
    l.wire(low,mux->getInputPinDynamic("A"));
    l.wire(high,mux->getInputPinDynamic("B"));
    return mux->getOutputPinDynamic("OUT");
}

class ArithmeticGroup : public IOComponent {
    std::string type_;
protected:
    size_t width_;
    ArithmeticGroup(std::string name, std::string type, size_t width,
                    const std::function<void(IOComponent*)>& pins)
        : IOComponent(std::move(name), pins), type_(std::move(type)+"<"+std::to_string(width)+">"), width_(width) {}
    P input(const std::string& name) { return getInputPinDynamic(name); }
    P output(const std::string& name) { return getOutputPinDynamic(name); }
public:
    const char* getTypeName() const override { return type_.c_str(); }
};

class IterationControl : public ArithmeticGroup {
public:
    IterationControl(std::string name, size_t width)
        : ArithmeticGroup(std::move(name), "ArithmeticIterationControl", width, [](IOComponent* p) {
            for (auto n : {"CLK","RST","ENABLE","CANCEL","START","TAKE"}) p->addPin(n,PinType::INPUT);
            for (auto n : {"READY","BUSY","DONE","LAUNCH","DATA_WRITE"}) p->addPin(n,PinType::OUTPUT);
        }) {}
    void buildInternals(ComponentBuilder& b) override {
        const size_t count_width=std::countr_zero(width_);
        Logic l(b);
        auto enable=input("ENABLE"), cancel=input("CANCEL");
        auto state_enable=l.lor({enable,cancel});
        auto busy=l.reg("BUSY_STATE",1,input("CLK"),input("RST"),state_enable);
        auto done=l.reg("DONE_STATE",1,input("CLK"),input("RST"),state_enable);
        auto count=l.reg("ITERATION",count_width,input("CLK"),input("RST"),enable);
        auto active=busy->getOutputPin("Q"), valid=done->getOutputPin("Q");
        auto ready=l.land({l.inv(l.lor({input("RST"),cancel})),l.inv(l.lor({active,valid}))});
        auto launch=l.land({enable,l.land({input("START"),ready})});
        auto last=l.land({active,l.all(l.split(count->getOutputPinDynamic("Q")))});
        l.wire(l.land({l.inv(cancel),l.lor({launch,l.land({active,l.inv(last)})})}),busy->getInputPin("D"));
        l.wire(l.land({l.inv(cancel),l.lor({last,l.land({valid,l.inv(input("TAKE"))})})}),done->getInputPin("D"));
        auto cv=l.split(count->getOutputPinDynamic("Q"));
        auto inc=l.add({cv,l.constant(count_width,1),l.bit(false)}).first;
        l.drive(l.mux({launch,l.mux({active,cv,inc}),l.constant(count_width)}),count->getInputPinDynamic("D"));
        l.out(ready,output("READY")); l.out(active,output("BUSY")); l.out(valid,output("DONE"));
        l.out(launch,output("LAUNCH"));
        l.out(l.land({enable,l.lor({launch,active})}),output("DATA_WRITE"));
        l.finish();
    }
};

class ArithmeticOperands : public ArithmeticGroup {
    bool divide_;
public:
    ArithmeticOperands(std::string name, size_t width, bool divide)
        : ArithmeticGroup(std::move(name),divide?"DivideOperands":"MultiplyOperands",width,[width](IOComponent* p) {
            for(auto n:{"CLK","RST","LAUNCH","SIGNED_A","SIGNED_B"}) p->addPin(n,PinType::INPUT);
            for(auto n:{"A","B"}) p->addPinDynamic(n,PinType::INPUT,width);
            for(auto n:{"OPERAND_A","OPERAND_B","INITIAL_LOW"}) p->addPinDynamic(n,PinType::OUTPUT,width);
            for(auto n:{"NEGATIVE_A","NEGATIVE_B"}) p->addPin(n,PinType::OUTPUT);
        }), divide_(divide) {}
    void buildInternals(ComponentBuilder& b) override {
        Logic l(b); auto a=l.split(input("A")), bv=l.split(input("B"));
        auto clk=input("CLK"), reset=input("RST"), launch=input("LAUNCH");
        auto ar=l.reg(divide_?"DIVISOR":"MULTIPLICAND",width_,clk,reset,launch);
        auto sa=l.reg(divide_?"QUOTIENT_NEGATIVE":"NEGATIVE_A",1,clk,reset,launch);
        auto sb=l.reg(divide_?"REMAINDER_NEGATIVE":"NEGATIVE_B",1,clk,reset,launch);
        auto na=l.land({input("SIGNED_A"),a.back()}), nb=l.land({input("SIGNED_B"),bv.back()});
        if(divide_) {
            l.wire(selectWord(b,l,"DIVISOR_MAGNITUDE",nb,input("B"),l.negateWord(input("B"))),ar->getInputPinDynamic("D"));
            l.wire(l.lx({na,nb}),sa->getInputPin("D")); l.wire(na,sb->getInputPin("D"));
            l.wire(selectWord(b,l,"DIVIDEND_MAGNITUDE",na,input("A"),l.negateWord(input("A"))),output("INITIAL_LOW"));
            l.out(ar->getOutputPinDynamic("Q"),output("OPERAND_A"));
            l.wire(l.constantWord(width_),output("OPERAND_B"));
        } else {
            auto br=l.reg("ORIGINAL_MULTIPLIER",width_,clk,reset,launch);
            l.drive(a,ar->getInputPinDynamic("D")); l.drive(bv,br->getInputPinDynamic("D"));
            l.wire(na,sa->getInputPin("D")); l.wire(nb,sb->getInputPin("D"));
            l.drive(bv,output("INITIAL_LOW"));
            l.out(ar->getOutputPinDynamic("Q"),output("OPERAND_A"));
            l.out(br->getOutputPinDynamic("Q"),output("OPERAND_B"));
        }
        l.out(sa->getOutputPin("Q"),output("NEGATIVE_A"));
        l.out(sb->getOutputPin("Q"),output("NEGATIVE_B")); l.finish();
    }
};

class IterativeDatapath : public ArithmeticGroup {
    bool divide_;
public:
    IterativeDatapath(std::string name,size_t width,bool divide)
        : ArithmeticGroup(std::move(name),divide?"DivideDatapath":"MultiplyDatapath",width,[width](IOComponent* p) {
            for(auto n:{"CLK","RST","WRITE","LAUNCH"}) p->addPin(n,PinType::INPUT);
            for(auto n:{"OPERAND","INITIAL_LOW"}) p->addPinDynamic(n,PinType::INPUT,width);
            for(auto n:{"LOW","HIGH"}) p->addPinDynamic(n,PinType::OUTPUT,width);
        }), divide_(divide) {}
    void buildInternals(ComponentBuilder& b) override {
        Logic l(b);
        auto q=l.reg("LOW_OR_QUOTIENT",width_,input("CLK"),input("RST"),input("WRITE"));
        auto r=l.reg("HIGH_OR_REMAINDER",width_,input("CLK"),input("RST"),input("WRITE"));
        auto qv=l.split(q->getOutputPinDynamic("Q"));
        auto step=b.add(divide_?circuit::families::divideStep(width_):circuit::families::multiplyStep(width_),"STEP");
        l.wire(r->getOutputPinDynamic("Q"),step->getInputPinDynamic(divide_?"REMAINDER":"HI"));
        l.wire(input("OPERAND"),step->getInputPinDynamic(divide_?"DIVISOR":"A"));
        l.wire(divide_?qv.back():qv.front(),step->getInputPin("BIT"));
        V nextq;
        P nextr;
        if(divide_) {
            nextq=l.shiftLeft(qv,step->getOutputPin("QUOTIENT_BIT"));
            nextr=step->getOutputPinDynamic("NEXT_REMAINDER");  // already a word
        } else {
            auto sum=l.split(step->getOutputPinDynamic("SUM"));
            nextq=l.shiftRight(qv,sum.front()); nextr=l.join(l.shiftRight(sum,step->getOutputPin("CARRY")));
        }
        l.wire(selectWord(b,l,"LOW_NEXT",input("LAUNCH"),l.join(nextq),input("INITIAL_LOW")),q->getInputPinDynamic("D"));
        l.wire(selectWord(b,l,"HIGH_NEXT",input("LAUNCH"),nextr,l.constantWord(width_)),r->getInputPinDynamic("D"));
        l.out(q->getOutputPinDynamic("Q"),output("LOW"));
        l.out(r->getOutputPinDynamic("Q"),output("HIGH")); l.finish();
    }
};

class ArithmeticResult : public ArithmeticGroup {
    bool divide_;
public:
    ArithmeticResult(std::string name,size_t width,bool divide)
        : ArithmeticGroup(std::move(name),divide?"DivideResultCorrection":"MultiplyResultCorrection",width,[width](IOComponent* p) {
            for(auto n:{"LOW","HIGH","OPERAND_A","OPERAND_B"}) p->addPinDynamic(n,PinType::INPUT,width);
            for(auto n:{"NEGATIVE_A","NEGATIVE_B"}) p->addPin(n,PinType::INPUT);
            for(auto n:{"LOW","HIGH"}) p->addPinDynamic(std::string("RESULT_")+n,PinType::OUTPUT,width);
        }), divide_(divide) {}
    void buildInternals(ComponentBuilder& b) override {
        // Word-level corrections: the words are negated or subtracted whole,
        // with the same inverters and adders, and never split.
        Logic l(b);
        if(divide_) {
            auto signedq=l.land({input("NEGATIVE_A"),l.any(l.split(input("OPERAND_A")))});
            l.wire(selectWord(b,l,"QUOTIENT_SIGN",signedq,input("LOW"),l.negateWord(input("LOW"))),output("RESULT_LOW"));
            l.wire(selectWord(b,l,"REMAINDER_SIGN",input("NEGATIVE_B"),input("HIGH"),l.negateWord(input("HIGH"))),output("RESULT_HIGH"));
        } else {
            auto zero=l.constantWord(width_);
            auto correct_a=selectWord(b,l,"SIGNED_A_CORRECTION",input("NEGATIVE_A"),zero,input("OPERAND_B"));
            auto correct_b=selectWord(b,l,"SIGNED_B_CORRECTION",input("NEGATIVE_B"),zero,input("OPERAND_A"));
            auto hi=l.subtractWords(input("HIGH"),correct_a);
            hi=l.subtractWords(hi,correct_b);
            l.wire(hi,output("RESULT_HIGH")); l.wire(input("LOW"),output("RESULT_LOW"));
        }
        l.finish();
    }
};

class MulDivResultSelection : public ArithmeticGroup {
public:
    MulDivResultSelection(std::string name,size_t width)
        : ArithmeticGroup(std::move(name),"MulDivResultSelection",width,[width](IOComponent* p) {
            p->addPin<3>("OP",PinType::INPUT);
            for(auto n:{"LO","HI","QUOTIENT","REMAINDER"}) p->addPinDynamic(n,PinType::INPUT,width);
            p->addPinDynamic("RESULT",PinType::OUTPUT,width);
        }) {}
    void buildInternals(ComponentBuilder& b) override {
        Logic l(b); auto op=l.split(input("OP"));
        auto product=selectWord(b,l,"PRODUCT_HALF",l.lor({op[0],op[1]}),input("LO"),input("HI"));
        auto division=selectWord(b,l,"DIVISION_HALF",op[1],input("QUOTIENT"),input("REMAINDER"));
        l.wire(selectWord(b,l,"RESULT",op[2],product,division),output("RESULT")); l.finish();
    }
};
} // namespace rv32im::detail
