#include "modules/composite/WordDatapath.hpp"
#include "components/ComponentBuilder.tpp"
#include "modules/composite/WordBehavioral.hpp"
#include "modules/composite/WordBuilder.hpp"
#include "modules/composite/ParallelPrefixAdder.hpp"
#include "modules/composite/FullAdder.hpp"
#include "components/StructuralLogic.hpp"
#include "components/selection/FamilyRegistration.hpp"
#include <map>
#include <tuple>
#include <cctype>

namespace circuit::word {
void buildALU(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildAddSub(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildArithmetic(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildLogic(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildShift(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildCompare(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildFlags(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildResult(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildWordZero(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildIntegerALU(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildIntegerAdder(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildIntegerAddSub(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildIntegerLogic(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildIntegerShift(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildIntegerCompare(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildIntegerFlags(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
void buildIntegerResult(IOComponent&,ComponentBuilder&,size_t,ALUConfig);
namespace {
void validate(Unit u,size_t width,ALUConfig c) {
    if(width!=8 && width!=16 && width!=32)throw std::invalid_argument("Word components support 8, 16 and 32 bits");
    if(c.operations!=Operations::Extended && c.operations!=Operations::IntegerDatapath)throw std::invalid_argument("Invalid ALU operation contract");
    if(c.adder!=AdderTopology::Ripple && c.adder!=AdderTopology::ParallelPrefix)throw std::invalid_argument("Invalid adder topology");
    if(u==Unit::ALU && c.operations==Operations::Extended && !c.full_flags)throw std::invalid_argument("Extended ALU requires its flag contract");
}
ALUConfig normalized(Unit u,ALUConfig c) {
    if(u==Unit::Adder || u==Unit::Zero || u==Unit::Arithmetic)c.operations=Operations::Extended;
    if(u!=Unit::ALU)c.full_flags=true;
    if(u!=Unit::ALU && u!=Unit::AddSub && u!=Unit::Arithmetic && u!=Unit::Adder)c.adder=AdderTopology::Ripple;
    return c;
}
std::string unitName(Unit u) {
    switch(u){case Unit::ALU:return "ALU";case Unit::AddSub:return "AddSub";case Unit::Arithmetic:return "ArithmeticUnit";
    case Unit::Logic:return "LogicUnit";case Unit::Shift:return "BarrelShifter";case Unit::Compare:return "CompareSelect";
    case Unit::Flags:return "ALUFlagUnit";case Unit::Result:return "ALUResultSelector";case Unit::Adder:return "Adder";case Unit::Zero:return "ZeroDetect";}
    throw std::invalid_argument("Invalid word unit");
}
std::string typeName(Unit u,size_t w,ALUConfig c) {
    c=normalized(u,c);std::string s=unitName(u)+"<"+std::to_string(w);
    if(u==Unit::ALU || (c.operations==Operations::IntegerDatapath && u!=Unit::Adder && u!=Unit::Zero))s+=c.operations==Operations::Extended?",extended":",integer";
    if(u==Unit::ALU || u==Unit::AddSub || u==Unit::Arithmetic || u==Unit::Adder)s+=c.adder==AdderTopology::Ripple?",ripple":",prefix";
    if(u==Unit::ALU && !c.full_flags)s+=",core";
    return s+">";
}
void pins(IOComponent* p,Unit u,size_t w,ALUConfig c) {
    validate(u,w,c);
    auto input=[&](const char* n,size_t width=1){p->addPinDynamic(n,PinType::INPUT,width);};
    auto output=[&](const char* n,size_t width=1){p->addPinDynamic(n,PinType::OUTPUT,width);};
    if(c.operations==Operations::IntegerDatapath && u!=Unit::ALU && u!=Unit::Adder && u!=Unit::Zero){
        if(u==Unit::Flags){input("RESULT",w);for(auto n:{"ARITHMETIC_ENABLE","ARITH_CARRY","ARITH_OVERFLOW"})input(n);
            for(auto n:{"ZERO","NEGATIVE","CARRY_OUT","OVERFLOW"})output(n);return;}
        if(u==Unit::Compare){input("DIFF",w);for(auto n:{"CARRY_IN","OVERFLOW_IN","ENABLE"})input(n);
            for(auto n:{"EQ","LT_SIGNED","LT_UNSIGNED"})output(n);output("SLT_RESULT",w);output("SLTU_RESULT",w);return;}
        input("A",w);input("B",w);
        if(u==Unit::Logic){for(auto n:{"AND_OUT","OR_OUT","XOR_OUT"})output(n,w);return;}
        output("OUT",w);
        if(u==Unit::AddSub){input("SUB");output("CARRY_OUT");output("OVERFLOW");return;}
        if(u==Unit::Shift){input("LEFT");input("ARITHMETIC");return;}
        if(u==Unit::Result){for(auto n:{"ARITHMETIC_RESULT","AND_RESULT","OR_RESULT","XOR_RESULT","SHIFT_RESULT","SLT_RESULT","SLTU_RESULT"})input(n,w);
            input("LOCAL_OP",3);input("HIGH_HALF");input("OUT_OF_RANGE");return;}
        throw std::invalid_argument("Invalid integer unit");
    }
    if(u==Unit::Zero){input("A",w);output("ZERO");return;}
    if(u==Unit::Flags){input("RESULT",w);for(auto n:{"ARITHMETIC","SHIFT","ARITH_CARRY","ARITH_OVERFLOW","SHIFT_CARRY"})input(n);
        for(auto n:{"ZERO","NEGATIVE","CARRY","OVERFLOW"})output(n);return;}
    input("A",w);input("B",w);
    if(u==Unit::Adder){input("Cin");output("Sum",w);output("Cout");return;}
    output("OUT",w);
    if(u==Unit::Result){for(auto n:{"ARITHMETIC_RESULT","LOGIC_RESULT","SHIFT_RESULT","COMPARE_RESULT"})input(n,w);
        for(auto n:{"ARITHMETIC_ENABLE","LOGIC_ENABLE","SHIFT_ENABLE","COMPARE_ENABLE","PASS_A_ENABLE","PASS_B_ENABLE"})input(n);return;}
    if(u==Unit::AddSub){input("SUB");input("C_IN");output("CARRY_OUT");output("OVERFLOW");return;}
    input("OP",u==Unit::ALU?5:3);
    if(u==Unit::Compare){input("DIFF",w);input("CARRY_IN");input("OVERFLOW_IN");return;}
    if(u==Unit::Logic)return;
    if(u==Unit::ALU && c.operations==Operations::IntegerDatapath){
        for(auto n:{"EQ","LT_SIGNED","LT_UNSIGNED"})output(n);
        if(c.full_flags)for(auto n:{"ZERO","NEGATIVE","CARRY_OUT","OVERFLOW"})output(n);return;
    }
    input("C_IN");output("CARRY");
    if(u==Unit::Shift)return;
    output("OVERFLOW");output("INVALID_OP");
    if(u==Unit::ALU){output("ZERO");output("NEGATIVE");}
}
std::string contractId(Unit u,size_t w,ALUConfig c) {
    std::string name=unitName(u);for(auto& ch:name)ch=static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    std::string id="word."+name+"."+std::to_string(w);
    if(u==Unit::ALU || c.operations==Operations::IntegerDatapath)id+=c.operations==Operations::Extended?".extended":".integer";
    if(u==Unit::ALU || u==Unit::AddSub || u==Unit::Arithmetic || u==Unit::Adder)id+=c.adder==AdderTopology::Ripple?".ripple":".prefix";
    if(u==Unit::ALU && !c.full_flags)id+=".core";
    return id;
}
struct Entry {
    std::string id,type; ComponentFamily family;
    Entry(Unit u,size_t w,ALUConfig c):id(contractId(u,w,c)),type(typeName(u,w,c)),
        family(id,type,[u,w,c](IOComponent* p){pins(p,u,w,c);},[u,w,c](const std::string& n,const std::shared_ptr<BuildContext>& context){
            return Component::createWithContext<WordComponent>(context,n,u,w,c);
        },behavioralFactory(u,w,c)){}
};

}
const ComponentFamily& family(Unit u,size_t width,ALUConfig c) {
    validate(u,width,c);c=normalized(u,c);
    using Key=std::tuple<Unit,size_t,Operations,AdderTopology,bool>;
    static std::map<Key,std::unique_ptr<Entry>> entries;
    Key key{u,width,c.operations,c.adder,c.full_flags};auto& entry=entries[key];
    if(!entry)entry=std::make_unique<Entry>(u,width,c);return entry->family;
}
WordComponent::WordComponent(std::string name,Unit u,size_t width,ALUConfig c)
    :IOComponent(std::move(name),[u,width,c](IOComponent* p){pins(p,u,width,normalized(u,c));}),unit_(u),width_(width),config_(normalized(u,c)),type_(typeName(u,width,c)) {validate(u,width,c);}
void WordComponent::buildInternals(ComponentBuilder& b){build(unit_,*this,b,width_,config_);}
void build(Unit u,IOComponent& self,ComponentBuilder& b,size_t width,ALUConfig c) {
    validate(u,width,c);
    if(u==Unit::Adder){buildIntegerAdder(self,b,width,c);return;}
    if(u==Unit::Zero){buildWordZero(self,b,width,c);return;}
    if(c.operations==Operations::IntegerDatapath){
        switch(u){case Unit::ALU:buildIntegerALU(self,b,width,c);return;case Unit::AddSub:buildIntegerAddSub(self,b,width,c);return;
        case Unit::Logic:buildIntegerLogic(self,b,width,c);return;case Unit::Shift:buildIntegerShift(self,b,width,c);return;
        case Unit::Compare:buildIntegerCompare(self,b,width,c);return;case Unit::Flags:buildIntegerFlags(self,b,width,c);return;case Unit::Result:buildIntegerResult(self,b,width,c);return;default:break;}
    }
    switch(u){case Unit::ALU:buildALU(self,b,width,c);break;case Unit::AddSub:buildAddSub(self,b,width,c);break;case Unit::Arithmetic:buildArithmetic(self,b,width,c);break;
    case Unit::Logic:buildLogic(self,b,width,c);break;case Unit::Shift:buildShift(self,b,width,c);break;case Unit::Compare:buildCompare(self,b,width,c);break;
    case Unit::Flags:buildFlags(self,b,width,c);break;case Unit::Result:buildResult(self,b,width,c);break;default:throw std::invalid_argument("Invalid word unit");}

}
std::string verificationOwner(Unit u,size_t width,ALUConfig c) {
    if(u==Unit::ALU && width==32 && c.operations==Operations::IntegerDatapath && c.adder==AdderTopology::ParallelPrefix && !c.full_flags) return "ALUIntegerCoreWidth32Test";
    if(u==Unit::Adder && width==8 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple) return "AdderRippleWidth8Test";
    if(u==Unit::AddSub && width==8 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple) return "AddSubExtendedWidth8Test";
    if(u==Unit::Arithmetic && width==8 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple) return "ArithmeticExtendedWidth8Test";
    if(u==Unit::Logic && width==8 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple) return "LogicExtendedWidth8Test";
    if(u==Unit::Shift && width==8 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple) return "ShiftExtendedWidth8Test";
    if(u==Unit::Zero && width==8 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple) return "ZeroDetectWidth8Test";
    if(u==Unit::Compare && width==8 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple) return "CompareExtendedWidth8Test";
    if(u==Unit::Flags && width==8 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple) return "FlagsExtendedWidth8Test";
    if(u==Unit::Result && width==8 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple) return "ResultSelectorExtendedWidth8Test";
    if(u==Unit::ALU && width==8 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple) return "ALUExtendedWidth8Test";
    if(u==Unit::Adder && width==32 && c.operations==Operations::Extended && c.adder==AdderTopology::ParallelPrefix) return "AdderPrefixWidth32Test";
    if(u==Unit::AddSub && width==32 && c.operations==Operations::IntegerDatapath && c.adder==AdderTopology::ParallelPrefix) return "AddSubIntegerWidth32Test";
    if(u==Unit::Logic && width==32 && c.operations==Operations::IntegerDatapath && c.adder==AdderTopology::Ripple) return "LogicIntegerWidth32Test";
    if(u==Unit::Shift && width==32 && c.operations==Operations::IntegerDatapath && c.adder==AdderTopology::Ripple) return "ShiftIntegerWidth32Test";
    if(u==Unit::Zero && width==32 && c.operations==Operations::Extended && c.adder==AdderTopology::Ripple) return "ZeroDetectWidth32Test";
    if(u==Unit::Compare && width==32 && c.operations==Operations::IntegerDatapath && c.adder==AdderTopology::Ripple) return "CompareIntegerWidth32Test";
    if(u==Unit::Flags && width==32 && c.operations==Operations::IntegerDatapath && c.adder==AdderTopology::Ripple) return "FlagsIntegerWidth32Test";
    if(u==Unit::Result && width==32 && c.operations==Operations::IntegerDatapath && c.adder==AdderTopology::Ripple) return "ResultSelectorIntegerWidth32Test";
    if(u==Unit::ALU && width==32 && c.operations==Operations::IntegerDatapath && c.adder==AdderTopology::ParallelPrefix) return "ALUIntegerWidth32Test";
    return (u==Unit::ALU || u==Unit::Adder)?"ALUWidthContractTest":"WidthComponentsTest";
}
void registerComponents(ComponentCatalog& catalog) {
    for(auto operations:{Operations::Extended,Operations::IntegerDatapath}) {
        const auto& f=controlFamily(operations);
        const auto test=operations==Operations::Extended?"ALUControlExtendedTest":"ALUControlIntegerTest";
        registration::registerStructural(catalog,f,test);
        if(f.supports(Fidelity::Behavioral))registration::registerBehavioral(catalog,f,test,test,{},"stable-after-settle");
    }
    for(size_t width:{8u,16u,32u})for(auto u:{Unit::ALU,Unit::AddSub,Unit::Arithmetic,Unit::Logic,Unit::Shift,Unit::Compare,Unit::Flags,Unit::Result,Unit::Adder,Unit::Zero})
        for(auto adder:{AdderTopology::Ripple,AdderTopology::ParallelPrefix})for(auto operations:{Operations::Extended,Operations::IntegerDatapath})for(bool flags:{true,false}) {
            if(u!=Unit::ALU && !flags)continue;
            if((u==Unit::Adder || u==Unit::Zero || u==Unit::Arithmetic) && operations!=Operations::Extended)continue;
            if(operations==Operations::Extended && !flags)continue;
            const auto config=normalized(u,{operations,adder,flags});
            const auto& f=family(u,width,config);
            if(catalog.hasContract(std::string(f.id())))continue;
            const auto test=verificationOwner(u,width,config);
            const bool binary=u==Unit::ALU && operations==Operations::IntegerDatapath && f.supports(Fidelity::Behavioral);
            registration::registerStructural(catalog,f,test,"stable-after-settle",{},{},binary?"known-binary":"four-state");
            if(f.supports(Fidelity::Behavioral))registration::registerBehavioral(catalog,f,test,test,{},"stable-after-settle",binary?"known-binary":"four-state");
        }
}

}

namespace circuit::families {
const ComponentFamily& alu(size_t w,word::ALUConfig c){return word::family(word::Unit::ALU,w,c);}
const ComponentFamily& adder(size_t w,word::AdderTopology t){return word::family(word::Unit::Adder,w,{word::Operations::Extended,t});}
const ComponentFamily& addSub(size_t w,word::ALUConfig c){return word::family(word::Unit::AddSub,w,c);}
const ComponentFamily& zeroDetect(size_t w){return word::family(word::Unit::Zero,w);}
}
