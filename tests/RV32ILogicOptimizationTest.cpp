#include "modules/composite/ALU32ResultSelector.hpp"
#include "../src/modules/composite/Structural8Builder.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "simulator/SimulationTest.hpp"
#include <iostream>
#include <random>

namespace {
using Bits=std::vector<LogicValue>;
using Values=std::map<std::string,Bits>;
void check(bool ok,const std::string& message) { if(!ok) throw std::runtime_error(message); }
Bits bits(uint32_t value,unsigned width=32) {
    Bits result(width);for(unsigned i=0;i<width;++i)result[i]=LogicValue((value>>i)&1);return result;
}

// Retain the former circuit, rather than deriving expectations from the new
// minterms. In particular, a gate mux does NOT resolve mux(1,1,X) to 1.
class SelectorReference : public ALU32ResultSelector {
public:
    using ALU32ResultSelector::ALU32ResultSelector;
    void buildInternals(ComponentBuilder& b) override {
        circuit::detail::StructuralBuilder n(*this,b);
        auto low=n.add<Mux8to1_32bit>("LOW"),high=n.add<Mux8to1_32bit>("HIGH");
        auto half=n.add<Mux2to1_32bit>("HALF"),range=n.add<Mux2to1_32bit>("RANGE");
        auto zero=n.constant<32>("ZERO",0);
        std::array<decltype(n)::Signal,8> lo{n.input("ARITHMETIC_RESULT"),n.input("ARITHMETIC_RESULT"),
            n.input("AND_RESULT"),n.input("OR_RESULT"),n.input("XOR_RESULT"),
            n.input("SHIFT_RESULT"),n.input("SHIFT_RESULT"),n.input("SHIFT_RESULT")};
        std::array<decltype(n)::Signal,8> hi{n.input("SLT_RESULT"),n.input("SLTU_RESULT"),n.input("A"),n.input("B"),zero,zero,zero,zero};
        for(unsigned i=0;i<8;++i) {
            n.connect(lo[i],low->getInputPin<32>("IN"+std::to_string(i)));
            n.connect(hi[i],high->getInputPin<32>("IN"+std::to_string(i)));
        }
        for(auto c:{low,high}) n.connect(n.input("LOCAL_OP"),c->getInputPin<3>("SEL"));
        n.connect(low->getOutputPin<32>("OUT"),half->getInputPin<32>("A"));
        n.connect(high->getOutputPin<32>("OUT"),half->getInputPin<32>("B"));
        n.connect(n.input("HIGH_HALF"),half->getInputPin("SEL"));
        n.connect(half->getOutputPin<32>("OUT"),range->getInputPin<32>("A"));
        n.connect(zero,range->getInputPin<32>("B"));
        n.connect(n.input("OUT_OF_RANGE"),range->getInputPin("SEL"));
        n.connect(range->getOutputPin<32>("OUT"),n.output("OUT"));n.finish();
    }
};
class NextPCReference : public RV32ISingleCycleNextPCStage {
public:
    using RV32ISingleCycleNextPCStage::RV32ISingleCycleNextPCStage;
    void buildInternals(ComponentBuilder& b) override {
        circuit::detail::StructuralWiring w(b);
        auto jump=b.addNewComponent<BitSplitter<2>>("JUMP");
        w.connect(getInputPin<2>("JUMP_TYPE"),jump->getInputPin<2>("IN"));
        auto jumping=w.either("JUMPING",jump->getOutputPin("OUT_0"),jump->getOutputPin("OUT_1"));
        auto taken=w.either("TAKEN",jumping,getInputPin("BRANCH_TAKEN"));
        auto select=b.addNewComponent<Mux2to1_32bit>("SELECT");
        w.connect(getInputPin<32>("PC_PLUS_4"),select->getInputPin<32>("A"));
        w.connect(getInputPin<32>("CONTROL_TARGET"),select->getInputPin<32>("B"));
        w.connect(taken,select->getInputPin("SEL"));w.connect(select->getOutputPin<32>("OUT"),getOutputPin<32>("NEXT_PC"));
        auto align=b.addNewComponent<RV32IMemoryAlignmentUnit>("ALIGNMENT");
        auto size=b.addNewComponent<ConstantValue<2>>("SIZE",2);
        w.connect(getInputPin<32>("CONTROL_TARGET"),align->getInputPin<32>("ADDRESS"));
        w.connect(size->getOutputPin<2>("OUT"),align->getInputPin<2>("SIZE"));
        w.connect(w.both("MISALIGNED",taken,align->getOutputPin("MISALIGNED")),getOutputPin("TARGET_MISALIGNED"));w.finish();
    }
};
struct Comparison {
    std::shared_ptr<IOComponent> root=Component::create<IOComponent>("ROOT"),fast,reference;
    std::map<std::string,std::shared_ptr<WireBase>> inputs;
    std::map<std::string,std::pair<std::shared_ptr<WireBase>,std::shared_ptr<WireBase>>> outputs;
    Simulator sim;
    size_t time=0,rows=0;
    Comparison(std::shared_ptr<IOComponent> f,std::shared_ptr<IOComponent> r):fast(f),reference(r) {
        root->addChild(fast);root->addChild(reference);ComponentBuilder b(root);
        for(const auto& [name,p]:fast->getAllInputPins())
            inputs[name]=b.addNewWireDynamic(name,p->getWidth(),nullptr,{p,reference->getInputPinDynamic(name)});
        for(const auto& [name,p]:fast->getAllOutputPins()) outputs[name]={
            b.addNewWireDynamic("fast_"+name,p->getWidth(),p,{}),
            b.addNewWireDynamic("ref_"+name,p->getWidth(),reference->getOutputPinDynamic(name),{})};
        sim.setHistoryRecordingEnabled(false);SimulationTest::scheduleInitialEventsForTree(root,sim,0);
        auto ft=circuit::analysis::CircuitTimingAnalyzer::analyze(fast);
        auto rt=circuit::analysis::CircuitTimingAnalyzer::analyze(reference);
        check(ft.valid&&rt.valid&&ft.critical_path_delay<=rt.critical_path_delay,"Combinational depth regressed");
        check(ft.combinational_component_count<rt.combinational_component_count,"Expected fewer gates");
        std::cout<<fast->getTypeName()<<": depth "<<rt.critical_path_delay<<" -> "<<ft.critical_path_delay
            <<", combinational nodes "<<rt.combinational_component_count<<" -> "<<ft.combinational_component_count<<'\n';
    }
    template<size_t W> void drive(const std::shared_ptr<WireBase>& wire,const Bits& value) {
        sim.scheduleEvent(std::make_shared<WireUpdateEvent<W>>(time,std::dynamic_pointer_cast<Wire<W>>(wire),value));
    }
    void row(const Values& values) {
        for(const auto& [name,value]:values) {
            auto wire=inputs.at(name);
            switch(value.size()) {case 1:drive<1>(wire,value);break;case 2:drive<2>(wire,value);break;
                case 3:drive<3>(wire,value);break;case 32:drive<32>(wire,value);break;
                default:throw std::runtime_error("Unsupported test width");}
        }
        sim.runAndRecord(time+100);check(!sim.hasPendingEvents(),"Circuit failed to settle");
        for(const auto& [name,pair]:outputs) check(pair.first->getValueVector()==pair.second->getValueVector(),
            "Four-state mismatch at row "+std::to_string(rows)+" output "+name);
        time+=150;++rows;
    }
};
void selector() {
    Comparison c(Component::create<ALU32ResultSelector>("FAST"),Component::create<SelectorReference>("REFERENCE"));
    std::mt19937 random(0x20260924);
    for(unsigned controls=0;controls<1024;++controls) for(unsigned pattern=0;pattern<4;++pattern) {
        Values in;
        in["LOCAL_OP"]={LogicValue(controls&3),LogicValue((controls>>2)&3),LogicValue((controls>>4)&3)};
        in["HIGH_HALF"]={LogicValue((controls>>6)&3)};in["OUT_OF_RANGE"]={LogicValue((controls>>8)&3)};
        for(const auto& [name,wire]:c.inputs) if(wire->getWidth()==32) {
            auto value=bits(pattern==0?0:pattern==1?0xffffffff:random());
            if(pattern==3)for(auto& bit:value)bit=LogicValue(random()%4);
            in[name]=value;
        }
        c.row(in);
    }
    std::cout<<c.rows<<" selector binary/four-state rows passed\n";
}
void nextPC() {
    Comparison c(Component::create<RV32ISingleCycleNextPCStage>("FAST"),Component::create<NextPCReference>("REFERENCE"));
    for(unsigned controls=0;controls<1024;++controls) {
        auto target=bits(0xa5a5a5a0);target[0]=LogicValue(controls&3);target[1]=LogicValue((controls>>2)&3);
        // Other address bits must not affect alignment, even when unknown.
        target[17]=LogicValue((controls>>4)&3);
        c.row({{"CONTROL_TARGET",target},{"PC_PLUS_4",bits(4)},
            {"BRANCH_TAKEN",{LogicValue((controls>>4)&3)}},
            {"JUMP_TYPE",{LogicValue((controls>>6)&3),LogicValue((controls>>8)&3)}}});
    }
    std::cout<<c.rows<<" next-PC binary/four-state rows passed\n";
}
}
int main() { try { selector();nextPC(); } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
