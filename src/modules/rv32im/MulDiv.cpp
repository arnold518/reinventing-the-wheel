#include "components/ComponentBuilder.tpp"
#include "modules/rv32im/MulDiv.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/FamilyFactories.hpp"
#include "RegisterComponent.hpp"
#include "MulDivBehavioral.hpp"
#include "components/StructuralLogic.hpp"
#include <stdexcept>
#include <bit>
using circuit::structural::Logic;
using circuit::structural::P;
using circuit::structural::V;
namespace {
void stepPins(IOComponent* p,size_t width,bool divide) {
    if(width!=4 && width!=8 && width!=16 && width!=32)throw std::invalid_argument("Arithmetic step supports widths 4, 8, 16 and 32");
    p->addPinDynamic(divide?"REMAINDER":"HI",PinType::INPUT,width);
    p->addPinDynamic(divide?"DIVISOR":"A",PinType::INPUT,width);
    p->addPin("BIT",PinType::INPUT);
    p->addPinDynamic(divide?"NEXT_REMAINDER":"SUM",PinType::OUTPUT,width);
    p->addPin(divide?"QUOTIENT_BIT":"CARRY",PinType::OUTPUT);
}
void unitPins(IOComponent* p,bool divide,bool combined=false,size_t width=32) {
    if(width!=4 && width!=8 && width!=16 && width!=32)throw std::invalid_argument("Invalid arithmetic width");
    for(auto s:{"CLK","RST","ENABLE","CANCEL","START","TAKE"})p->addPin(s,PinType::INPUT);
    for(auto s:{"READY","BUSY","DONE"})p->addPin(s,PinType::OUTPUT);
    for(auto s:{"A","B"})p->addPinDynamic(s,PinType::INPUT,width);
    if(combined){p->addPin<3>("OP",PinType::INPUT);p->addPinDynamic("RESULT",PinType::OUTPUT,width);}
    else if(divide){p->addPin("SIGNED",PinType::INPUT);for(auto s:{"QUOTIENT","REMAINDER"})p->addPinDynamic(s,PinType::OUTPUT,width);}
    else {for(auto s:{"SIGNED_A","SIGNED_B"})p->addPin(s,PinType::INPUT);for(auto s:{"LO","HI"})p->addPinDynamic(s,PinType::OUTPUT,width);}
}
void buildUnit(IOComponent& p,ComponentBuilder& b,bool divide,size_t width=32) {
    const size_t count_width=std::countr_zero(width);
    Logic l(b);auto in=[&](const char* s){return p.getInputPinDynamic(s);};
    auto clk=in("CLK"),enable=in("ENABLE");
    auto reset=in("RST"),cancel=in("CANCEL");
    // Cancellation is a synchronous ownership transition, including while
    // paused. Only external reset drives the asynchronous reset network.
    auto state_enable=l.lor({enable,cancel});
    auto busy=l.reg("BUSY_STATE",1,clk,reset,state_enable);
    auto done=l.reg("DONE_STATE",1,clk,reset,state_enable);
    auto count=l.reg("ITERATION",count_width,clk,reset,enable);
    auto active=busy->getOutputPin("Q"),valid=done->getOutputPin("Q");
    auto ready=l.land({l.inv(l.lor({reset,cancel})),l.inv(l.lor({active,valid}))});
    auto launch=l.land({enable,l.land({in("START"),ready})});
    auto last=l.land({active,l.all(l.split(count->getOutputPinDynamic("Q")))});
    l.wire(l.land({l.inv(cancel),l.lor({launch,l.land({active,l.inv(last)})})}),busy->getInputPin("D"));
    l.wire(l.land({l.inv(cancel),l.lor({last,l.land({valid,l.inv(in("TAKE"))})})}),done->getInputPin("D"));
    auto cv=l.split(count->getOutputPinDynamic("Q"));
    auto inc=l.add({cv,l.constant(count_width,1),l.bit(false)}).first;
    l.drive(l.mux({launch,l.mux({active,cv,inc}),l.constant(count_width)}),count->getInputPinDynamic("D"));
    auto data_we=l.land({enable,l.lor({launch,active})});
    auto input_we=launch;
    auto a=l.split(in("A")),bv=l.split(in("B"));
    auto q=l.reg("LOW_OR_QUOTIENT",width,clk,reset,data_we);
    auto r=l.reg("HIGH_OR_REMAINDER",width,clk,reset,data_we);
    auto qv=l.split(q->getOutputPinDynamic("Q")),rv=l.split(r->getOutputPinDynamic("Q"));
    V nextq,nextr;
    if(!divide) {
        auto ar=l.reg("MULTIPLICAND",width,clk,reset,input_we);
        auto br=l.reg("ORIGINAL_MULTIPLIER",width,clk,reset,input_we);
        auto sa=l.reg("NEGATIVE_A",1,clk,reset,input_we);
        auto sb=l.reg("NEGATIVE_B",1,clk,reset,input_we);
        l.drive(a,ar->getInputPinDynamic("D"));l.drive(bv,br->getInputPinDynamic("D"));
        l.wire(l.land({in("SIGNED_A"),a[width-1]}),sa->getInputPin("D"));
        l.wire(l.land({in("SIGNED_B"),bv[width-1]}),sb->getInputPin("D"));
        auto step=b.add(circuit::families::multiplyStep(width),"STEP");
        l.wire(r->getOutputPinDynamic("Q"),step->getInputPinDynamic("HI"));
        l.wire(ar->getOutputPinDynamic("Q"),step->getInputPinDynamic("A"));
        l.wire(qv[0],step->getInputPin("BIT"));
        auto sum=l.split(step->getOutputPinDynamic("SUM"));
        nextq=l.shiftRight(qv,sum[0]);nextr=l.shiftRight(sum,step->getOutputPin("CARRY"));
        auto hi=l.sub({rv,l.mux({sa->getOutputPin("Q"),l.constant(width),l.split(br->getOutputPinDynamic("Q"))})}).first;
        hi=l.sub({hi,l.mux({sb->getOutputPin("Q"),l.constant(width),l.split(ar->getOutputPinDynamic("Q"))})}).first;
        l.drive(hi,p.getOutputPinDynamic("HI"));l.drive(qv,p.getOutputPinDynamic("LO"));
        l.drive(l.mux({launch,nextq,bv}),q->getInputPinDynamic("D"));
    } else {
        auto dr=l.reg("DIVISOR",width,clk,reset,input_we);
        auto ns=l.reg("QUOTIENT_NEGATIVE",1,clk,reset,input_we);
        auto nr=l.reg("REMAINDER_NEGATIVE",1,clk,reset,input_we);
        auto na=l.land({in("SIGNED"),a[width-1]}),nb=l.land({in("SIGNED"),bv[width-1]});
        l.drive(l.mux({nb,bv,l.negate(bv)}),dr->getInputPinDynamic("D"));
        l.wire(l.lx({na,nb}),ns->getInputPin("D"));l.wire(na,nr->getInputPin("D"));
        auto step=b.add(circuit::families::divideStep(width),"STEP");
        l.wire(r->getOutputPinDynamic("Q"),step->getInputPinDynamic("REMAINDER"));
        l.wire(dr->getOutputPinDynamic("Q"),step->getInputPinDynamic("DIVISOR"));
        l.wire(qv[width-1],step->getInputPin("BIT"));
        nextq=l.shiftLeft(qv,step->getOutputPin("QUOTIENT_BIT"));
        nextr=l.split(step->getOutputPinDynamic("NEXT_REMAINDER"));
        auto signedq=l.land({ns->getOutputPin("Q"),l.any(l.split(dr->getOutputPinDynamic("Q")))});
        l.drive(l.mux({signedq,qv,l.negate(qv)}),p.getOutputPinDynamic("QUOTIENT"));
        l.drive(l.mux({nr->getOutputPin("Q"),rv,l.negate(rv)}),p.getOutputPinDynamic("REMAINDER"));
        l.drive(l.mux({launch,nextq,l.mux({na,a,l.negate(a)})}),q->getInputPinDynamic("D"));
    }
    l.drive(l.mux({launch,nextr,l.constant(width)}),r->getInputPinDynamic("D"));
    l.out(ready,p.getOutputPin("READY"));l.out(active,p.getOutputPin("BUSY"));l.out(valid,p.getOutputPin("DONE"));
    l.finish();
}
}
namespace circuit::families {
const ComponentFamily& multiplyStep(size_t width) {
    static const ComponentFamily small{"arithmetic.multiply-step.4","MultiplyStep<4>",[](IOComponent*p){stepPins(p,4,false);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::MultiplyStep>(c,n,4);}};
    static const ComponentFamily word{"arithmetic.multiply-step.32","MultiplyStep<32>",[](IOComponent*p){stepPins(p,32,false);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::MultiplyStep>(c,n,32);}};
    static const ComponentFamily w8{"arithmetic.multiply-step.8","MultiplyStep<8>",[](IOComponent*p){stepPins(p,8,false);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::MultiplyStep>(c,n,8);}};
    static const ComponentFamily w16{"arithmetic.multiply-step.16","MultiplyStep<16>",[](IOComponent*p){stepPins(p,16,false);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::MultiplyStep>(c,n,16);}};
    if(width==8)return w8;if(width==16)return w16;if(width==4)return small;if(width==32)return word;throw std::invalid_argument("Invalid multiply step width");
}
const ComponentFamily& divideStep(size_t width) {
    static const ComponentFamily small{"arithmetic.divide-step.4","DivideStep<4>",[](IOComponent*p){stepPins(p,4,true);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::DivideStep>(c,n,4);}};
    static const ComponentFamily word{"arithmetic.divide-step.32","DivideStep<32>",[](IOComponent*p){stepPins(p,32,true);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::DivideStep>(c,n,32);}};
    static const ComponentFamily w8{"arithmetic.divide-step.8","DivideStep<8>",[](IOComponent*p){stepPins(p,8,true);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::DivideStep>(c,n,8);}};
    static const ComponentFamily w16{"arithmetic.divide-step.16","DivideStep<16>",[](IOComponent*p){stepPins(p,16,true);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::DivideStep>(c,n,16);}};
    if(width==8)return w8;if(width==16)return w16;if(width==4)return small;if(width==32)return word;throw std::invalid_argument("Invalid divide step width");
}

}
MultiplyStep::MultiplyStep(std::string name,size_t width):IOComponent(std::move(name),[width](IOComponent*p){stepPins(p,width,false);}),width_(width),type_("MultiplyStep<"+std::to_string(width)+">"){}
void MultiplyStep::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto a=l.split(getInputPinDynamic("A"));for(auto&x:a)x=l.land({x,getInputPin("BIT")});
    auto [sum,carry]=l.add({l.split(getInputPinDynamic("HI")),a,l.bit(false)});
    l.drive(sum,getOutputPinDynamic("SUM"));l.out(carry,getOutputPin("CARRY"));l.finish();
}
DivideStep::DivideStep(std::string name,size_t width):IOComponent(std::move(name),[width](IOComponent*p){stepPins(p,width,true);}),width_(width),type_("DivideStep<"+std::to_string(width)+">"){}
void DivideStep::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto r=l.split(getInputPinDynamic("REMAINDER"));auto high=r.back();
    r=l.shiftLeft(r,getInputPin("BIT"));auto [difference,carry]=l.sub({r,l.split(getInputPinDynamic("DIVISOR"))});
    auto ge=l.lor({high,carry});l.drive(l.mux({ge,r,difference}),getOutputPinDynamic("NEXT_REMAINDER"));
    l.out(ge,getOutputPin("QUOTIENT_BIT"));l.finish();
}
namespace {
void buildCombined(IOComponent& self,ComponentBuilder& b,size_t width) {
    Logic l(b);auto mul=b.add(circuit::families::multiplier(width),"MULTIPLY");
    auto div=b.add(circuit::families::divider(width),"DIVIDE");
    for(auto s:{"CLK","RST","ENABLE","CANCEL","A","B","TAKE"})for(auto u:{mul,div})l.wire(self.getInputPinDynamic(s),u->getInputPinDynamic(s));
    auto ready=l.land({mul->getOutputPin("READY"),div->getOutputPin("READY")});
    auto start=l.land({self.getInputPin("ENABLE"),l.land({self.getInputPin("START"),ready})});
    auto op=l.split(self.getInputPin<3>("OP"));
    auto saved=l.reg("OPERATION",3,self.getInputPin("CLK"),self.getInputPin("RST"),start);
    l.wire(self.getInputPin<3>("OP"),saved->getInputPin<3>("D"));
    l.wire(l.land({start,l.inv(op[2])}),mul->getInputPin("START"));l.wire(l.land({start,op[2]}),div->getInputPin("START"));
    l.wire(l.lx({op[0],op[1]}),mul->getInputPin("SIGNED_A"));l.wire(l.land({op[0],l.inv(op[1])}),mul->getInputPin("SIGNED_B"));
    l.wire(l.inv(op[0]),div->getInputPin("SIGNED"));
    auto held=l.split(saved->getOutputPin<3>("Q"));
    auto m=l.mux({l.lor({held[0],held[1]}),l.split(mul->getOutputPinDynamic("LO")),l.split(mul->getOutputPinDynamic("HI"))});
    auto d=l.mux({held[1],l.split(div->getOutputPinDynamic("QUOTIENT")),l.split(div->getOutputPinDynamic("REMAINDER"))});
    l.drive(l.mux({held[2],m,d}),self.getOutputPinDynamic("RESULT"));
    l.out(ready,self.getOutputPin("READY"));
    l.out(l.lor({mul->getOutputPin("BUSY"),div->getOutputPin("BUSY")}),self.getOutputPin("BUSY"));
    l.out(l.lor({mul->getOutputPin("DONE"),div->getOutputPin("DONE")}),self.getOutputPin("DONE"));l.finish();
}
}

namespace rv32im {
void registerArithmetic(circuit::ComponentCatalog& c) {
    for(size_t width:{4u,8u,16u,32u}) {
        detail::registerStructural(c,circuit::families::multiplyStep(width),"MultiplyStepTest");
        detail::registerStructural(c,circuit::families::divideStep(width),"DivideStepTest");
        const auto owner=[&](const char* kind){return width==32?std::string(kind)+"Width32Test":std::string("WidthComponentsTest");};
        for(const auto& entry:std::vector<std::pair<const circuit::ComponentFamily*,std::string>>{
            {&circuit::families::multiplier(width),owner("Multiplier")},
            {&circuit::families::divider(width),owner("Divider")},
            {&circuit::families::mulDivUnit(width),owner("MulDivUnit")}}) {
            detail::registerStructural(c,*entry.first,entry.second,"clock-boundary");
        }
    }
    const auto& f=circuit::families::mulDivUnit(32);
    circuit::ImplementationDescriptor compact;
    compact.id=std::string(f.id())+".behavioral";compact.contract_id=f.id();
    compact.fidelity=circuit::Fidelity::Behavioral;compact.concrete_type_name=f.typeName();
    compact.supports=[](const circuit::ParameterMap& p){return p.empty();};
    compact.factory=[](const std::string& n,const circuit::ParameterMap&,const std::shared_ptr<circuit::BuildContext>& context){
        return detail::createMulDivBehavioral(n,context,32);
    };
    compact.evidence={circuit::VerificationStatus::Characterized,
        {"iterative multiplier/divider gates and characterized register state"},{"MulDivUnitWidth32Test"},
        {"MulDivUnitWidth32ScenarioTest","RV32IMFiveStageCoreTest"},{},"four-state","clock-boundary"};
    c.registerImplementation(std::move(compact));
}
}

namespace circuit::arithmetic {
IterativeUnit::IterativeUnit(std::string name,size_t width,Operation operation)
    :IOComponent(std::move(name),[width,operation](IOComponent* p){unitPins(p,operation==Operation::Divide,operation==Operation::MulDiv,width);}),
     width_(width),operation_(operation),type_((operation==Operation::Multiply?"Multiplier<":operation==Operation::Divide?"Divider<":"MulDivUnit<")+std::to_string(width)+">") {if(operation!=Operation::Multiply && operation!=Operation::Divide && operation!=Operation::MulDiv)throw std::invalid_argument("Invalid arithmetic operation");}
void IterativeUnit::buildInternals(ComponentBuilder& b){
    if(operation_==Operation::MulDiv)buildCombined(*this,b,width_);
    else buildUnit(*this,b,operation_==Operation::Divide,width_);
}
}
namespace circuit::families {
namespace {
template<size_t W,circuit::arithmetic::Operation Op> const ComponentFamily& iterativeFamily(){
    using namespace circuit::arithmetic;
    static const std::string kind=Op==Operation::Multiply?"multiplier":Op==Operation::Divide?"divider":"mul-div";
    static const std::string id="arithmetic.iterative."+kind+"."+std::to_string(W);
    static const std::string type=(Op==Operation::Multiply?"Multiplier<":Op==Operation::Divide?"Divider<":"MulDivUnit<")+std::to_string(W)+">";
    static const ComponentFamily f{id,type,[](IOComponent* p){unitPins(p,Op==Operation::Divide,Op==Operation::MulDiv,W);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<IterativeUnit>(c,n,W,Op);},
        Op==Operation::MulDiv && W==32?FamilyFactory([](const std::string& n,const std::shared_ptr<BuildContext>& c){return rv32im::detail::createMulDivBehavioral(n,c,W);}):FamilyFactory{}};
    return f;
}
template<circuit::arithmetic::Operation Op> const ComponentFamily& iterativeWidth(size_t w){
    switch(w){case 4:return iterativeFamily<4,Op>();case 8:return iterativeFamily<8,Op>();case 16:return iterativeFamily<16,Op>();
    case 32:return iterativeFamily<32,Op>();}
    throw std::invalid_argument("Invalid iterative arithmetic width");
}
}
const ComponentFamily& multiplier(size_t w){return iterativeWidth<circuit::arithmetic::Operation::Multiply>(w);}
const ComponentFamily& divider(size_t w){return iterativeWidth<circuit::arithmetic::Operation::Divide>(w);}
const ComponentFamily& mulDivUnit(size_t w){return iterativeWidth<circuit::arithmetic::Operation::MulDiv>(w);}
}
