#include "modules/composite/WordRecipes.hpp"
#include "modules/composite/WordALUValueModel.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.tpp"
#include <algorithm>

namespace circuit::word {
namespace {
constexpr std::array<const char*,6> enables{
    "ARITHMETIC_ENABLE","LOGIC_ENABLE","SHIFT_ENABLE","COMPARE_ENABLE","PASS_A_ENABLE","PASS_B_ENABLE"};

// Compact form of one Chapter 2 unit: the settled values of its own gates.
template<size_t W>
class WordBehavioral final : public BasicComponent {
    using Value=ALUValueModel<W,Carry::Prefix>;
    using Ripple=ALUValueModel<W,Carry::Ripple>;
    Unit unit_; std::string type_;
    template<size_t N> typename Value::template Bits<N> read(const char* name) {
        const auto bits=getInputPin<N>(name)->getValueAsVector();
        typename Value::template Bits<N> out;
        std::copy(bits.begin(),bits.end(),out.begin());return out;
    }
    LogicValue bit(const char* name){return getInputPin(name)->getValue();}
    template<size_t N> void write(Simulator& s,SimulationTime t,const char* name,const typename Value::template Bits<N>& bits) {
        _updateOutputWireDynamic(s,name,{bits.begin(),bits.end()},t);
    }
    void write(Simulator& s,SimulationTime t,const char* name,LogicValue value){_updateOutputWire(s,name,value,t);}
    void flags(Simulator& s,SimulationTime t,const typename Value::Flags& f) {
        write(s,t,"ZERO",f.zero);write(s,t,"NEGATIVE",f.negative);
        write(s,t,"CARRY",f.carry);write(s,t,"OVERFLOW",f.overflow);
    }
public:
    WordBehavioral(std::string name,Unit unit)
        :BasicComponent(std::move(name),1,family(unit,W).pinInitializer()),
         unit_(unit),type_(family(unit,W).typeName()){}
    const char* getTypeName()const override{return type_.c_str();}
    void evaluate(SimulationTime t,Simulator& s)override {
        switch(unit_) {
        case Unit::RippleAdder: {
            const auto r=Ripple::add(read<W>("A"),read<W>("B"),bit("C_IN"));
            write<W>(s,t,"OUT",r.out);write(s,t,"C_OUT",r.carry);break;
        }
        case Unit::PrefixAdder: {
            const auto r=Value::add(read<W>("A"),read<W>("B"),bit("C_IN"));
            write<W>(s,t,"OUT",r.out);write(s,t,"C_OUT",r.carry);write(s,t,"C_SIGN",r.carry_into_sign);break;
        }
        case Unit::Zero:write(s,t,"ZERO",Value::zero(read<W>("A")));break;
        case Unit::AddSub: {
            const auto r=Value::addsub(read<W>("A"),read<W>("B"),bit("SUB"),bit("C_IN"));
            write<W>(s,t,"OUT",r.out);write(s,t,"C_OUT",r.carry);write(s,t,"OVERFLOW",r.overflow);break;
        }
        case Unit::Arithmetic: {
            const auto r=Value::arithmetic(read<W>("A"),read<W>("B"),read<3>("OP"),bit("C_IN"));
            write<W>(s,t,"OUT",r.out);write(s,t,"CARRY",r.carry);write(s,t,"OVERFLOW",r.overflow);
            write(s,t,"INVALID_OP",r.invalid);break;
        }
        case Unit::Logic:write<W>(s,t,"OUT",Value::logic(read<W>("A"),read<W>("B"),read<3>("OP")));break;
        case Unit::Shift: {
            const auto r=Value::shift(read<W>("A"),read<W>("B"),read<3>("OP"),bit("C_IN"));
            write<W>(s,t,"OUT",r.out);write(s,t,"CARRY",r.carry);break;
        }
        case Unit::Compare:
            write<W>(s,t,"OUT",Value::compare(read<W>("A"),read<W>("B"),read<W>("DIFF"),
                bit("CARRY_IN"),bit("OVERFLOW_IN"),read<3>("OP")));break;
        case Unit::Flags:
            flags(s,t,Value::flags(read<W>("RESULT"),bit("ARITHMETIC"),bit("SHIFT"),
                bit("ARITH_CARRY"),bit("ARITH_OVERFLOW"),bit("SHIFT_CARRY")));break;
        case Unit::Result: {
            typename Value::template Bits<6> enabled;
            for(size_t i=0;i<enables.size();++i)enabled[i]=bit(enables[i]);
            write<W>(s,t,"OUT",Value::select({read<W>("ARITHMETIC_RESULT"),read<W>("LOGIC_RESULT"),
                read<W>("SHIFT_RESULT"),read<W>("COMPARE_RESULT"),read<W>("A"),read<W>("B")},enabled));break;
        }
        case Unit::ALU: {
            const auto r=Value::alu(read<W>("A"),read<W>("B"),read<5>("OP"),bit("C_IN"));
            write<W>(s,t,"OUT",r.out);flags(s,t,r.flags);write(s,t,"INVALID_OP",r.invalid);break;
        }
        }
    }
};

class ControlBehavioral final : public BasicComponent {
public:
    explicit ControlBehavioral(std::string name)
        :BasicComponent(std::move(name),1,controlFamily().pinInitializer()){}
    const char* getTypeName()const override{return "ALUControlDecoder";}
    void evaluate(SimulationTime t,Simulator& s)override {
        using Value=ALUValueModel<8>;
        Value::Bits<5> op;const auto in=getInputPin<5>("OP")->getValueAsVector();
        std::copy(in.begin(),in.end(),op.begin());const auto c=Value::control(op);
        _updateOutputWireDynamic(s,"LOCAL_OP",{c.local.begin(),c.local.end()},t);
        _updateOutputWireDynamic(s,"ARITH_OP",{c.arithmetic.begin(),c.arithmetic.end()},t);
        _updateOutputWireDynamic(s,"COMPARE_OP",{c.compare.begin(),c.compare.end()},t);
        for(size_t i=0;i<enables.size();++i)_updateOutputWire(s,enables[i],c.enables[i],t);
        _updateOutputWire(s,"INVALID_OP",c.invalid,t);
    }
};

template<size_t W> FamilyFactory factory(Unit u) {
    return [u](const std::string& n,const std::shared_ptr<BuildContext>& context){
        return Component::createWithContext<WordBehavioral<W>>(context,n,u);
    };
}
}

// Compact forms exist where they are characterized: every unit at 8 bits,
// both adders at every width, and zero detection at 32 bits.
FamilyFactory behavioralFactory(Unit u,size_t width) {
    const bool adder=u==Unit::RippleAdder || u==Unit::PrefixAdder;
    if(width==8)return factory<8>(u);
    if(width==16 && adder)return factory<16>(u);
    if(width==32 && (adder || u==Unit::Zero))return factory<32>(u);
    return {};
}
FamilyFactory controlBehavioralFactory() {
    return [](const std::string& n,const std::shared_ptr<BuildContext>& c){
        return Component::createWithContext<ControlBehavioral>(c,n);
    };
}
}
