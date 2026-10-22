#include "modules/composite/WordBehavioral.hpp"
#include "modules/composite/WordALUValueModel.hpp"
#include "modules/composite/IntegerALUValue.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.tpp"
#include "basic/LogicValueHelpers.hpp"
#include <algorithm>

namespace circuit::word {
namespace {
constexpr std::array<const char*,6> enables{
    "ARITHMETIC_ENABLE","LOGIC_ENABLE","SHIFT_ENABLE","COMPARE_ENABLE","PASS_A_ENABLE","PASS_B_ENABLE"};

template<size_t W,AdderTopology Topology>
class WordBehavioral final : public BasicComponent {
    using Value=ExtendedALUValueModel<W,Topology>;
    using Word=typename Value::Word;
    Unit unit_; ALUConfig config_; std::string type_;
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
    void extended(Simulator& s,SimulationTime t) {
        switch(unit_) {
        case Unit::Adder: {
            const auto r=Value::add(read<W>("A"),read<W>("B"),bit("Cin"));
            write<W>(s,t,"Sum",r.out);write(s,t,"Cout",r.carry);break;
        }
        case Unit::Zero:write(s,t,"ZERO",Value::zero(read<W>("A")));break;
        case Unit::AddSub: {
            const auto r=Value::addsub(read<W>("A"),read<W>("B"),bit("SUB"),bit("C_IN"));
            write<W>(s,t,"OUT",r.out);write(s,t,"CARRY_OUT",r.carry);write(s,t,"OVERFLOW",r.overflow);break;
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
    void integer(Simulator& s,SimulationTime t) {
        if(unit_==Unit::Logic) {
            const auto a=read<W>("A"),b=read<W>("B");Word land,lor,lxor;
            for(size_t i=0;i<W;++i){land[i]=Value::land(a[i],b[i]);lor[i]=Value::lor(a[i],b[i]);lxor[i]=Value::lxor(a[i],b[i]);}
            write<W>(s,t,"AND_OUT",land);write<W>(s,t,"OR_OUT",lor);write<W>(s,t,"XOR_OUT",lxor);return;
        }
        if(unit_==Unit::AddSub) {
            const auto a=read<W>("A");auto b=read<W>("B");const auto sub=bit("SUB");
            for(auto& value:b)value=Value::lxor(value,sub);
            const auto r=Value::add(a,b,sub);
            write<W>(s,t,"OUT",r.out);write(s,t,"CARRY_OUT",r.carry);
            write(s,t,"OVERFLOW",Value::lxor(r.carry_into_sign,r.carry));return;
        }
        if(unit_==Unit::Shift) {
            const auto a=read<W>("A"),b=read<W>("B");
            const auto left=bit("LEFT"),fill=Value::land(Value::land(a[W-1],bit("ARITHMETIC")),Value::inv(left));
            Word current;
            for(size_t i=0;i<W;++i)current[i]=Value::mux(a[i],a[W-1-i],left);
            for(size_t stage=0;stage<std::countr_zero(W);++stage) {
                const size_t distance=size_t{1}<<stage;const auto previous=current;
                for(size_t i=0;i<W;++i)current[i]=Value::mux(previous[i],i+distance<W?previous[i+distance]:fill,b[stage]);
            }
            const auto previous=current;
            for(size_t i=0;i<W;++i)current[i]=Value::mux(previous[i],previous[W-1-i],left);
            write<W>(s,t,"OUT",current);return;
        }
        // The compact integer ALU has a known-binary equivalence domain.
        // Operand/control X/Z produces unknown outputs, as stated by its contract.
        if(unit_!=Unit::ALU)throw std::logic_error("No compact integer implementation for this unit");
        bool known=true;
        for(const char* name:{"A","B","OP"})known&=circuit::logic_values::allKnown(getInputPinDynamic(name)->getValueAsVector());
        if(!known) {
            for(const auto& [name,pin]:getAllOutputPins())
                _updateOutputWireDynamic(s,name,std::vector<LogicValue>(pin->getWidth(),LogicValue::UNKNOWN),t);
            return;
        }
        const uint32_t a=getInputPin<W>("A")->getValueAsUInt64(),b=getInputPin<W>("B")->getValueAsUInt64();
        const uint8_t op=getInputPin<5>("OP")->getValueAsUInt64();
        const auto out=integer_value::result<W>(op,a,b);
        _updateOutputWire<W>(s,"OUT",out,t);
        const bool compare=op==1||op==8||op==9;
        using circuit::logic_values::logic;
        write(s,t,"EQ",logic(compare&&a==b));
        write(s,t,"LT_SIGNED",logic(compare&&integer_value::signedValue<W>(a)<integer_value::signedValue<W>(b)));
        write(s,t,"LT_UNSIGNED",logic(compare&&a<b));
        if(config_.full_flags) {
            const bool arithmetic=op==0||compare;
            const auto ar=integer_value::addsub<W>(a,b,compare);
            write(s,t,"ZERO",logic(out==0));write(s,t,"NEGATIVE",logic(bool(out&integer_value::sign<W>)));
            write(s,t,"CARRY_OUT",logic(arithmetic&&ar.carry));write(s,t,"OVERFLOW",logic(arithmetic&&ar.overflow));
        }
    }
public:
    WordBehavioral(std::string name,Unit unit,ALUConfig config)
        :BasicComponent(std::move(name),1,family(unit,W,config).pinInitializer()),
         unit_(unit),config_(config),type_(family(unit,W,config).typeName()){}
    const char* getTypeName()const override{return type_.c_str();}
    void evaluate(SimulationTime t,Simulator& s)override {
        if(config_.operations==Operations::IntegerDatapath && unit_!=Unit::Adder && unit_!=Unit::Zero)integer(s,t);
        else extended(s,t);
    }
};

class ControlBehavioral final : public BasicComponent {
    std::string type_;
public:
    explicit ControlBehavioral(std::string name)
        :BasicComponent(std::move(name),1,controlFamily(Operations::Extended).pinInitializer()),
         type_(controlFamily(Operations::Extended).typeName()){}
    const char* getTypeName()const override{return type_.c_str();}
    void evaluate(SimulationTime t,Simulator& s)override {
        using Value=ExtendedALUValueModel<8>;
        Value::Bits<5> op;const auto in=getInputPin<5>("OP")->getValueAsVector();
        std::copy(in.begin(),in.end(),op.begin());const auto c=Value::control(op);
        _updateOutputWireDynamic(s,"LOCAL_OP",{c.local.begin(),c.local.end()},t);
        _updateOutputWireDynamic(s,"ARITH_OP",{c.arithmetic.begin(),c.arithmetic.end()},t);
        _updateOutputWireDynamic(s,"COMPARE_OP",{c.compare.begin(),c.compare.end()},t);
        for(size_t i=0;i<enables.size();++i)_updateOutputWire(s,enables[i],c.enables[i],t);
        _updateOutputWire(s,"INVALID_OP",c.invalid,t);
    }
};
}

FamilyFactory behavioralFactory(Unit u,size_t width,ALUConfig c) {
    if(width==8 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple)
        return [u,c](const std::string& n,const std::shared_ptr<BuildContext>& context){
            return Component::createWithContext<WordBehavioral<8,AdderTopology::Ripple>>(context,n,u,c);
        };
    const bool supported=width==32 && (u==Unit::Zero || (u==Unit::Adder&&c.adder==AdderTopology::ParallelPrefix) ||
        (c.operations==Operations::IntegerDatapath && (u==Unit::Logic || u==Unit::Shift ||
            ((u==Unit::ALU||u==Unit::AddSub)&&c.adder==AdderTopology::ParallelPrefix))));
    if(supported)return [u,c](const std::string& n,const std::shared_ptr<BuildContext>& context){
        return Component::createWithContext<WordBehavioral<32,AdderTopology::ParallelPrefix>>(context,n,u,c);
    };
    return {};
}
FamilyFactory controlBehavioralFactory(Operations operations) {
    if(operations==Operations::Extended)return [](const std::string& n,const std::shared_ptr<BuildContext>& c){
        return Component::createWithContext<ControlBehavioral>(c,n);
    };
    return {};
}
}
