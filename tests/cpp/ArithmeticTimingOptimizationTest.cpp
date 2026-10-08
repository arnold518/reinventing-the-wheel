#include "modules/composite/Adder32.hpp"
#include "modules/composite/AddSub32.hpp"
#include "modules/composite/ParallelPrefixAdder32.hpp"
#include "modules/composite/IncrementBy4.hpp"
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "simulator/SimulationTest.hpp"
#include <iostream>
#include <random>

namespace {
using Bits=std::vector<LogicValue>;
void check(bool v,const std::string& s){if(!v)throw std::runtime_error(s);}
Bits bits(uint32_t v){Bits r(32);for(unsigned i=0;i<32;++i)r[i]=(v>>i)&1?LogicValue::HIGH:LogicValue::LOW;return r;}
struct Comparison {
    std::shared_ptr<IOComponent> root=Component::create<IOComponent>("ROOT");
    std::shared_ptr<IOComponent> fast,reference;
    std::map<std::string,std::shared_ptr<WireBase>> inputs;
    std::map<std::string,std::pair<std::shared_ptr<WireBase>,std::shared_ptr<WireBase>>> outputs;
    Simulator sim;
    size_t time=0,rows=0;
    Comparison(bool subtract) {
        fast=subtract?std::static_pointer_cast<IOComponent>(Component::create<AddSub32>("FAST"))
                     :std::static_pointer_cast<IOComponent>(Component::create<Adder32>("FAST"));
        reference=subtract?std::static_pointer_cast<IOComponent>(Component::create<AddSub32>("RIPPLE",false))
                          :std::static_pointer_cast<IOComponent>(Component::create<Adder32>("RIPPLE",false));
        root->addChild(fast);root->addChild(reference);ComponentBuilder b(root);
        for(const auto& [n,p]:fast->getAllInputPins())inputs[n]=b.addNewWireDynamic(n,p->getWidth(),nullptr,{p,reference->getInputPinDynamic(n)});
        for(const auto& [n,p]:fast->getAllOutputPins())outputs[n]={b.addNewWireDynamic("fast_"+n,p->getWidth(),p,{}),
            b.addNewWireDynamic("ripple_"+n,p->getWidth(),reference->getOutputPinDynamic(n),{})};
        sim.setHistoryRecordingEnabled(false);SimulationTest::scheduleInitialEventsForTree(root,sim,0);
        const auto f=circuit::analysis::CircuitTimingAnalyzer::settling(fast);
        const auto r=circuit::analysis::CircuitTimingAnalyzer::settling(reference);
        check(f.valid&&r.valid&&f.bound<r.bound,"Prefix must shorten the settling bound");
        std::cout<<(subtract?"AddSub32":"Adder32")<<": ripple "<<r.bound<<", prefix "<<f.bound<<" ticks\n";
    }
    void row(const Bits& a,const Bits& b,LogicValue carry,bool subtract) {
        for(const auto& [name,value]:std::map<std::string,Bits>{{"A",a},{"B",b},{subtract?"SUB":"Cin",{carry}}}) {
            auto wire=inputs.at(name);
            if(value.size()==1)sim.scheduleEvent(std::make_shared<WireUpdateEvent<1>>(time,std::dynamic_pointer_cast<Wire<1>>(wire),value));
            else sim.scheduleEvent(std::make_shared<WireUpdateEvent<32>>(time,std::dynamic_pointer_cast<Wire<32>>(wire),value));
        }
        sim.runAndRecord(time+300);
        check(!sim.hasPendingEvents(),"Arithmetic did not settle");
        for(const auto& [name,pair]:outputs)check(pair.first->getValueVector()==pair.second->getValueVector(),
            "Four-state prefix/ripple mismatch at row "+std::to_string(rows)+" pin "+name);
        time+=400;++rows;
    }
};
void arithmetic(bool subtract) {
    Comparison c(subtract);
    for(unsigned n=0;n<1024;++n) {
        auto a=bits(0xfffffffc),b=bits(0);
        for(unsigned i=0;i<2;++i){a[i]=LogicValue((n>>(2*i))&3);b[i]=LogicValue((n>>(4+2*i))&3);}
        c.row(a,b,LogicValue(n>>8),subtract);
    }
    std::mt19937 random(0x20260923);
    for(unsigned n=0;n<2048;++n) {
        auto a=bits(random()),b=bits(random());
        if(n&1)for(unsigned i=0;i<32;++i){a[i]=LogicValue(random()%4);b[i]=LogicValue(random()%4);}
        c.row(a,b,LogicValue(random()%4),subtract);
    }
    for(unsigned i=0;i<32;++i)for(unsigned carry=0;carry<2;++carry) {
        c.row(bits((uint32_t{1}<<i)-1),bits(1),LogicValue(carry),subtract);
        c.row(bits(0x80000000),bits(0x7fffffff),LogicValue(carry),subtract);
    }
    std::cout<<c.rows<<" binary/four-state arithmetic rows passed\n";
}
void incrementByFour() {
    auto root=Component::create<IOComponent>("ROOT");
    auto increment=Component::create<IncrementBy4>("INCREMENT");
    auto reference=Component::create<Adder32>("REFERENCE");
    root->addChild(increment);root->addChild(reference);
    ComponentBuilder builder(root);
    auto input=builder.addNewWire<32>("INPUT",nullptr,
        {increment->getInputPin<32>("A"),reference->getInputPin<32>("A")});
    auto four=builder.addNewWire<32>("FOUR",nullptr,{reference->getInputPin<32>("B")});
    auto zero=builder.addNewWire("ZERO",nullptr,{reference->getInputPin("Cin")});
    auto actual=builder.addNewWire<32>("ACTUAL",increment->getOutputPin<32>("Sum"),{});
    auto expected=builder.addNewWire<32>("EXPECTED",reference->getOutputPin<32>("Sum"),{});
    Simulator simulator;
    simulator.setHistoryRecordingEnabled(false);
    SimulationTest::scheduleInitialEventsForTree(root,simulator,0);
    simulator.scheduleEvent(std::make_shared<WireUpdateEvent<32>>(0,four,bits(4)));
    simulator.scheduleEvent(std::make_shared<WireUpdateEvent<1>>(0,zero,Bits{LogicValue::LOW}));
    std::mt19937 random(0x20260930);
    size_t time=0;
    for (size_t row=0;row<256;++row) {
        uint32_t value=row<32?((uint32_t{1}<<row)-4):random();
        simulator.scheduleEvent(std::make_shared<WireUpdateEvent<32>>(time,input,bits(value)));
        simulator.runAndRecord(time+300);
        check(actual->getValueVector()==expected->getValueVector(),
            "PC increment mismatch at row "+std::to_string(row));
        time+=400;
    }
    const auto compact=circuit::analysis::CircuitTimingAnalyzer::settling(increment);
    check(compact.valid,"IncrementBy4 timing graph is invalid");
    std::cout<<"IncrementBy4: 256 boundary/random rows passed; settling bound "
             <<compact.bound<<" ticks\n";
}
}
int main(){try{arithmetic(false);arithmetic(true);incrementByFour();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
