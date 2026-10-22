#include "modules/composite/WordDatapath.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/composite/Incrementer.hpp"
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/TimingProfile.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "simulator/SimulationTest.hpp"
#include <iostream>
#include <random>
#include <string>

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
    SimulationTime time=0; size_t rows=0;
    Comparison(bool subtract) {
        fast=subtract?std::static_pointer_cast<IOComponent>(Component::create<AddSub<32>>("FAST",circuit::word::ALUConfig{circuit::word::Operations::IntegerDatapath,circuit::word::AdderTopology::ParallelPrefix,true}))
                     :std::static_pointer_cast<IOComponent>(Component::create<Adder<32>>("FAST",circuit::word::AdderTopology::ParallelPrefix));
        reference=subtract?std::static_pointer_cast<IOComponent>(Component::create<AddSub<32>>("RIPPLE",circuit::word::ALUConfig{circuit::word::Operations::IntegerDatapath,circuit::word::AdderTopology::Ripple,true}))
                          :std::static_pointer_cast<IOComponent>(Component::create<Adder<32>>("RIPPLE",circuit::word::AdderTopology::Ripple));
        root->addChild(fast);root->addChild(reference);ComponentBuilder b(root);
        for(const auto& [n,p]:fast->getAllInputPins())inputs[n]=b.addNewWireDynamic(n,p->getWidth(),nullptr,{p,reference->getInputPinDynamic(n)});
        for(const auto& [n,p]:fast->getAllOutputPins())outputs[n]={b.addNewWireDynamic("fast_"+n,p->getWidth(),p,{}),
            b.addNewWireDynamic("ripple_"+n,p->getWidth(),reference->getOutputPinDynamic(n),{})};
        sim.setHistoryRecordingEnabled(false);SimulationTest::scheduleInitialEventsForTree(root,sim,0);
        const auto f=circuit::analysis::CircuitTimingAnalyzer::settling(fast);
        const auto r=circuit::analysis::CircuitTimingAnalyzer::settling(reference);
        check(f.valid&&r.valid&&f.bound<r.bound,"Prefix must shorten the settling bound");
        std::cout<<(subtract?"AddSub<32,integer,prefix>":"Adder<32,prefix>")<<": ripple "<<r.bound<<", prefix "<<f.bound<<" ticks\n";
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
    auto increment=Component::create<Incrementer<32,4>>("INCREMENT");
    auto reference=Component::create<Adder<32>>("REFERENCE");
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
    SimulationTime time=0;
    for (size_t row=0;row<256;++row) {
        uint32_t value=row<32?((uint32_t{1}<<row)-4):random();
        simulator.scheduleEvent(std::make_shared<WireUpdateEvent<32>>(time,input,bits(value)));
        simulator.runAndRecord(time+300);
        check(actual->getValueVector()==expected->getValueVector(),
            "PC increment mismatch at row "+std::to_string(row));
        time+=400;
    }
    const auto compact=circuit::analysis::CircuitTimingAnalyzer::settling(increment);
    check(compact.valid,"Incrementer<32,4> timing graph is invalid");
    std::cout<<"Incrementer<32,4>: 256 boundary/random rows passed; settling bound "
             <<compact.bound<<" ticks\n";
}

template<size_t W>void incrementConfigurations() {
    const uint64_t mask=(uint64_t{1}<<W)-1;
    for(uint64_t step:{uint64_t{0},uint64_t{1},uint64_t{3},uint64_t{4},mask}) {
        auto root=Component::create<IOComponent>("ROOT");
        auto increment=Component::create<circuit::word::WordIncrementer>("INCREMENT",W,step);
        root->addChild(increment);ComponentBuilder builder(root);
        auto input=builder.addNewWire<W>("INPUT",nullptr,{increment->template getInputPin<W>("A")});
        auto output=builder.addNewWire<W>("OUTPUT",increment->template getOutputPin<W>("Sum"),{});
        Simulator simulator;simulator.setHistoryRecordingEnabled(false);
        SimulationTest::scheduleInitialEventsForTree(root,simulator,0);
        SimulationTime time=0;
        for(uint64_t value:{uint64_t{0},uint64_t{1},mask,mask-1,mask>>1,uint64_t{1}<<(W-1)}) {
            Bits stimulus(W),expected(W);
            for(size_t bit=0;bit<W;++bit){stimulus[bit]=(value>>bit)&1?LogicValue::HIGH:LogicValue::LOW;expected[bit]=((value+step)>>bit)&1?LogicValue::HIGH:LogicValue::LOW;}
            simulator.scheduleEvent(std::make_shared<WireUpdateEvent<W>>(time,input,stimulus));
            simulator.runAndRecord(time+300);time+=400;
            check(output->getValueVector()==expected,"Generic incrementer binary result");
            check(circuit::word::incrementWord(stimulus,step)==expected,"Generic compact increment binary result");
            for(auto unknown:{LogicValue::UNKNOWN,LogicValue::HIGH_Z}) {
                stimulus[0]=unknown;stimulus[W/2]=unknown;
                simulator.scheduleEvent(std::make_shared<WireUpdateEvent<W>>(time,input,stimulus));
                simulator.runAndRecord(time+300);time+=400;
                check(output->getValueVector()==circuit::word::incrementWord(stimulus,step),"Generic incrementer gate/value four-state equivalence");
            }
        }
    }
}

size_t prefixLevels(const std::shared_ptr<Component>& component) {
    size_t count = std::string(component->getTypeName()).starts_with("CarryPrefixLevel<");
    for (const auto& child : component->getChildren()) count += prefixLevels(child);
    return count;
}

struct WordTiming {
    SimulationTime bound;
    size_t prefix_levels;
};

WordTiming wordTiming(const circuit::ComponentFamily& family, const char* profile,
                      size_t width, const char* unit, const char* operations,
                      bool full_flags, bool prefix) {
    auto built = circuit::builtinComponentCatalog().createRoot(
        family.request("WORD"), circuit::strictAllStructural());
    const auto report = circuit::analysis::CircuitTimingAnalyzer::settling(built.root);
    check(report.valid && report.bound > 0, "Invalid generic word settling graph");
    const auto levels = prefixLevels(built.root);
    std::cout << "WORD_TIMING profile=" << profile << " width=" << width
              << " unit=" << unit << " operations=" << operations
              << " full_flags=" << full_flags << " prefix=" << prefix
              << " bound=" << report.bound << " prefix_levels=" << levels << '\n';
    return {report.bound, levels};
}

void genericWordTiming() {
    // Four-state sources remain unrestricted. These are conservative settling
    // bounds, not exact minimum delays, hold proofs or physical clock rates.
    using namespace circuit::word;
    const std::string previous_profile(circuit::timing::activeTimingProfile().name);
    for (const char* profile : {"unit", "embedded", "desktop"}) {
        circuit::timing::setActiveTimingProfile(profile);
        for (size_t width : {8u, 16u, 32u}) {
            const auto ripple = wordTiming(circuit::families::adder(width, AdderTopology::Ripple),
                profile, width, "adder", "shared", true, false);
            const auto prefix = wordTiming(circuit::families::adder(width, AdderTopology::ParallelPrefix),
                profile, width, "adder", "shared", true, true);
            check(prefix.bound < ripple.bound, "Configured prefix adder must shorten the settling bound");
            check(prefix.prefix_levels > ripple.prefix_levels, "Adder topology configuration was ignored");
            for (auto operations : {Operations::Extended, Operations::IntegerDatapath}) {
                for (bool full_flags : {true, false}) {
                    if (operations == Operations::Extended && !full_flags) continue;
                    const char* name = operations == Operations::Extended ? "extended" : "integer";
                    const auto slow = wordTiming(circuit::families::alu(width,
                        {operations, AdderTopology::Ripple, full_flags}),
                        profile, width, "alu", name, full_flags, false);
                    const auto fast = wordTiming(circuit::families::alu(width,
                        {operations, AdderTopology::ParallelPrefix, full_flags}),
                        profile, width, "alu", name, full_flags, true);
                    check(fast.prefix_levels > slow.prefix_levels, "ALU adder topology configuration was ignored");
                }
            }
        }
    }
    circuit::timing::setActiveTimingProfile(previous_profile);
}
}
int main(){try{arithmetic(false);arithmetic(true);incrementByFour();incrementConfigurations<8>();incrementConfigurations<16>();incrementConfigurations<32>();genericWordTiming();}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
