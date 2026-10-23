#include "tests/WordALUTests.hpp"
#include "tests/ComponentTestModel.hpp"
#include "tests/TestRegistry.hpp"
#include "modules/composite/ALU.hpp"
#include "modules/composite/RippleCarryAdder.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/composite/WordALUValueModel.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/StandardProfiles.hpp"
#include <algorithm>
#include <array>
#include <iostream>

namespace {
using namespace circuit::test;
using namespace circuit::word;

// Independent binary arithmetic, shared by the recorded examples and checks.
// The production four-state model is compared separately against the gates.
NamedValues expected(size_t w, uint64_t a, uint64_t b, unsigned op, bool cin) {
    const uint64_t mask = (uint64_t{1} << w) - 1, sign = uint64_t{1} << (w-1);
    auto signedValue = [=](uint64_t x) { return int64_t(x) - (x & sign ? int64_t(mask+1) : 0); };
    uint64_t out = 0;
    bool carry = false, overflow = false;
    auto add = [&](uint64_t x, uint64_t y, bool c) {
        const auto total = x + y + c;
        out = total & mask; carry = total > mask;
        overflow = (~(x ^ y) & (x ^ out) & sign) != 0;
    };
    const unsigned n = b & (w-1);
    switch (op) {
    case 0: add(a,b,false); break; case 1: add(a,b,cin); break;
    case 2: add(a,b^mask,true); break; case 3: add(a,b^mask,cin); break;
    case 4: add(a,0,true); break; case 5: add(a,mask-1,true); break;
    case 6: add(0,a^mask,true); break;
    case 8: out=a&b; break; case 9: out=a|b; break; case 10: out=a^b; break;
    case 11: out=a^mask; break; case 12: out=(a&b)^mask; break;
    case 13: out=(a|b)^mask; break; case 14: out=(a^b)^mask; break;
    case 16: out=(a<<n)&mask; carry=n ? (a>>(w-n))&1 : cin; break;
    case 17: out=a>>n; carry=n ? (a>>(n-1))&1 : cin; break;
    case 18: out=uint64_t(signedValue(a)>>n)&mask; carry=n ? (a>>(n-1))&1 : cin; break;
    case 19: out=((a<<n)|(a>>(w-n)))&mask; carry=n ? (a>>(w-n))&1 : cin; break;
    case 20: out=((a>>n)|(a<<(w-n)))&mask; carry=n ? (a>>(n-1))&1 : cin; break;
    case 21: out=a==b; break; case 22: out=a<b; break;
    case 23: out=signedValue(a)<signedValue(b); break;
    case 24: out=std::min(a,b); break; case 25: out=std::max(a,b); break;
    case 26: out=signedValue(a)<signedValue(b) ? a:b; break;
    case 27: out=signedValue(a)<signedValue(b) ? b:a; break;
    case 28: out=a; break; case 29: out=b; break; default: break;
    }
    return {{"OUT",logicBits(w,out)}, {"ZERO",logicBit(out==0)},
        {"NEGATIVE",logicBit(bool(out&sign))}, {"CARRY",logicBit(carry)},
        {"OVERFLOW",logicBit(overflow)}, {"INVALID_OP",logicBit(op==7||op==15||op==31)}};
}

NamedValues inputs(size_t w, uint64_t a, uint64_t b, unsigned op, bool cin) {
    return {{"A",logicBits(w,a)}, {"B",logicBits(w,b)}, {"OP",logicBits(5,op)}, {"C_IN",logicBit(cin)}};
}

template<size_t W>
NamedValues modeled(const NamedValues& in) {
    using Model = ALUValueModel<W,Carry::Prefix>;
    auto bits = [&]<size_t N>(const char* name) {
        typename Model::template Bits<N> result;
        std::copy(in.at(name).begin(),in.at(name).end(),result.begin());
        return result;
    };
    const auto r=Model::alu(bits.template operator()<W>("A"),bits.template operator()<W>("B"),
        bits.template operator()<5>("OP"),in.at("C_IN")[0]);
    return {{"OUT",{r.out.begin(),r.out.end()}}, {"ZERO",logicBit(r.flags.zero)},
        {"NEGATIVE",logicBit(r.flags.negative)}, {"CARRY",logicBit(r.flags.carry)},
        {"OVERFLOW",logicBit(r.flags.overflow)}, {"INVALID_OP",logicBit(r.invalid)}};
}

class GateFixture {
    std::shared_ptr<IOComponent> root_;
    std::map<std::string,std::shared_ptr<WireBase>> wires_;
    Simulator simulator_;
    SimulationTime time_=0;
public:
    explicit GateFixture(const circuit::ComponentFamily& family) {
        auto built=circuit::builtinComponentCatalog().createRoot(family.request("DUT"),circuit::strictAllStructural());
        root_=std::dynamic_pointer_cast<IOComponent>(built.root);
        ComponentBuilder builder(root_);
        for(const auto& [n,p]:root_->getAllInputPins()) wires_[n]=builder.addNewWireDynamic("IN_"+n,p->getWidth(),nullptr,{p});
        for(const auto& [n,p]:root_->getAllOutputPins()) builder.addNewWireDynamic("OUT_"+n,p->getWidth(),p,{});
        simulator_.setHistoryRecordingEnabled(false);
        SimulationTest::scheduleInitialEventsForTree(root_,simulator_);
    }
    void check(const NamedValues& in,const NamedValues& out) {
        for(const auto& [n,v]:in) simulator_.scheduleEvent(makeTestWireUpdate(time_+1,wires_.at(n),TestValue(v)));
        simulator_.advanceAndRecord(time_+=1000);
        for(const auto& [n,v]:out) if(root_->getOutputPinDynamic(n)->getValueAsVector()!=v)
            throw std::runtime_error("Generic ALU mismatch: "+n+" at "+std::to_string(time_));
    }
};

template<size_t W> void checkConfiguration() {
    GateFixture gates(circuit::families::alu(W));
    const uint64_t mask=(uint64_t{1}<<W)-1, sign=uint64_t{1}<<(W-1);
    auto checkBinary=[&](uint64_t a,uint64_t b,unsigned op,bool cin) {
        const auto in=inputs(W,a,b,op,cin), out=expected(W,a,b,op,cin);
        if(modeled<W>(in)!=out) throw std::runtime_error("Width-aware value rule differs from binary reference");
        gates.check(in,out);
    };
    for(auto [a,b]:std::array<std::pair<uint64_t,uint64_t>,4>{{{0,0},{mask,1},{sign,mask},{sign-1,1}}})
        for(unsigned op=0;op<32;++op) for(bool cin:{false,true}) checkBinary(a,b,op,cin);
    for(unsigned n=0;n<W;++n) for(unsigned op=16;op<=20;++op) for(bool cin:{false,true})
        checkBinary(sign|1,n,op,cin);
    for(auto unknown:{LogicValue::UNKNOWN,LogicValue::HIGH_Z}) {
        for(unsigned op:{0u,1u,2u,8u,16u,21u,26u,28u,30u,31u}) {
            auto in=inputs(W,mask,1,op,true);
            in["A"][W/2]=unknown; in["B"][W-1]=unknown; in["C_IN"][0]=unknown;
            gates.check(in,modeled<W>(in));
        }
        for(unsigned bit=0;bit<5;++bit) {
            auto in=inputs(W,sign,mask,0,false); in["OP"][bit]=unknown;
            gates.check(in,modeled<W>(in));
        }
    }
    std::cout<<"ALU<"<<W<<">: binary reference, all shift distances and targeted four-state rules agree\n";
}

class ALUWidthContractTest final : public StandaloneVerificationTest {
public:
    std::string getTestName()const override{return "ALUWidthContractTest";}
    void verifyResults()override {
        checkConfiguration<8>(); checkConfiguration<16>(); checkConfiguration<32>();
    }
};

ComponentTestSpec aluSpec(const std::string& name,size_t w) {
    ComponentTestSpec spec{name,std::string(circuit::families::alu(w).id()),"ALU",{},{{"contract",{},{},{}}}};
    const uint64_t mask=(uint64_t{1}<<w)-1, sign=uint64_t{1}<<(w-1);
    unsigned index=0;
    for(auto [a,b]:std::array<std::pair<uint64_t,uint64_t>,2>{{{mask,1},{sign,w-1}}})
        for(unsigned op=0;op<32;++op) {
            const bool cin=index>=32;
            spec.scenarios[0].actions.push_back({"case-"+std::to_string(index++),CheckpointKind::Settled,
                inputs(w,a,b,op,cin),expected(w,a,b,op,cin),true,{}});
        }
    return spec;
}

ComponentTestSpec adderSpec(const std::string& name,const circuit::ComponentFamily& family) {
    constexpr size_t W=16;
    ComponentTestSpec spec{name,std::string(family.id()),"ADDER",{},{{"contract",{},{},{}}}};
    unsigned index=0;
    for(auto [a,b]:std::array<std::pair<uint64_t,uint64_t>,4>{{{0,0},{0xffff,1},{0x7fff,1},{0x1234,0x4321}}})
        for(bool cin:{false,true}) {
            const uint64_t total=a+b+cin;
            spec.scenarios[0].actions.push_back({"sum-"+std::to_string(index++),CheckpointKind::Settled,
                {{"A",logicBits(W,a)},{"B",logicBits(W,b)},{"C_IN",logicBit(cin)}},
                {{"OUT",logicBits(W,total)},{"C_OUT",logicBit(total>0xffff)}},true,{}});
        }
    return spec;
}
}

void appendWordALUTests(std::vector<TestRegistryEntry>& entries) {
    entries.push_back({"ALUWidthContractTest","ALUWidthContractTest",RegisteredTestKind::Integration,{},
        {},{"simulation","alu","width"},[]{return std::make_unique<ALUWidthContractTest>();},false});
    for(auto [name,w]:std::array<std::pair<std::string,size_t>,3>{{{"ALUScenarioTest",16},{"ALUWidth8ScenarioTest",8},{"ALUWidth32ScenarioTest",32}}}) {
        const std::string contract(circuit::families::alu(w).id());
        entries.push_back({name,"ALUWidthContractTest",RegisteredTestKind::TruthTable,{contract},"contract",{"simulation","alu","width"},
            [name,w]{return std::make_unique<ComponentScenarioTest>(aluSpec(name,w),"contract",circuit::strictAllStructural());},true});
    }
    for(auto [name,family]:std::array<std::pair<std::string,const circuit::ComponentFamily*>,2>{{
            {"RippleCarryAdderWidth16Test",&circuit::families::rippleCarryAdder(16)},
            {"ParallelPrefixAdderWidth16Test",&circuit::families::parallelPrefixAdder(16)}}}) {
        const std::string contract(family->id());
        entries.push_back({name,"ALUWidthContractTest",RegisteredTestKind::TruthTable,{contract},"contract",{"simulation","alu","width"},
            [name,family]{return std::make_unique<ComponentScenarioTest>(adderSpec(name,*family),"contract",circuit::strictAllStructural());},true});
    }
}
