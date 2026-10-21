#include "components/ComponentBuilder.tpp"
#include "modules/rv32im/MulDiv.hpp"
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/FamilyFactories.hpp"
#include "RegisterComponent.hpp"
#include "MulDivBehavioral.hpp"
#include "StructuralLogic.hpp"
#include <stdexcept>
using rv32im::detail::Logic;
using rv32im::detail::P;
using rv32im::detail::V;
namespace {
void stepPins(IOComponent* p,size_t width,bool divide) {
    if(width!=4 && width!=32)throw std::invalid_argument("Arithmetic step supports widths 4 and 32");
    p->addPinDynamic(divide?"REMAINDER":"HI",PinType::INPUT,width);
    p->addPinDynamic(divide?"DIVISOR":"A",PinType::INPUT,width);
    p->addPin("BIT",PinType::INPUT);
    p->addPinDynamic(divide?"NEXT_REMAINDER":"SUM",PinType::OUTPUT,width);
    p->addPin(divide?"QUOTIENT_BIT":"CARRY",PinType::OUTPUT);
}
void unitPins(IOComponent* p,bool divide,bool combined=false) {
    for(auto s:{"CLK","RST","ENABLE","CANCEL","START","TAKE"})p->addPin(s,PinType::INPUT);
    for(auto s:{"READY","BUSY","DONE"})p->addPin(s,PinType::OUTPUT);
    for(auto s:{"A","B"})p->addPin<32>(s,PinType::INPUT);
    if(combined){p->addPin<3>("OP",PinType::INPUT);p->addPin<32>("RESULT",PinType::OUTPUT);}
    else if(divide){p->addPin("SIGNED",PinType::INPUT);for(auto s:{"QUOTIENT","REMAINDER"})p->addPin<32>(s,PinType::OUTPUT);}
    else {for(auto s:{"SIGNED_A","SIGNED_B"})p->addPin(s,PinType::INPUT);for(auto s:{"LO","HI"})p->addPin<32>(s,PinType::OUTPUT);}
}
void buildUnit(IOComponent& p,ComponentBuilder& b,bool divide) {
    Logic l(b);auto in=[&](const char* s){return p.getInputPinDynamic(s);};
    auto clk=in("CLK"),enable=in("ENABLE");
    auto reset=in("RST"),cancel=in("CANCEL");
    // Cancellation is a synchronous ownership transition, including while
    // paused. Only external reset drives the asynchronous reset network.
    auto state_enable=l.lor({enable,cancel});
    auto busy=l.reg("BUSY_STATE",1,clk,reset,state_enable);
    auto done=l.reg("DONE_STATE",1,clk,reset,state_enable);
    auto count=l.reg("ITERATION",5,clk,reset,enable);
    auto active=busy->getOutputPin("Q"),valid=done->getOutputPin("Q");
    auto ready=l.land({l.inv(l.lor({reset,cancel})),l.inv(l.lor({active,valid}))});
    auto launch=l.land({enable,l.land({in("START"),ready})});
    auto last=l.land({active,l.all(l.split(count->getOutputPin<5>("Q")))});
    l.wire(l.land({l.inv(cancel),l.lor({launch,l.land({active,l.inv(last)})})}),busy->getInputPin("D"));
    l.wire(l.land({l.inv(cancel),l.lor({last,l.land({valid,l.inv(in("TAKE"))})})}),done->getInputPin("D"));
    auto cv=l.split(count->getOutputPin<5>("Q"));
    auto inc=l.add({cv,l.constant(5,1),l.bit(false)}).first;
    l.drive(l.mux({launch,l.mux({active,cv,inc}),l.constant(5)}),count->getInputPin<5>("D"));
    auto data_we=l.land({enable,l.lor({launch,active})});
    auto input_we=launch;
    auto a=l.split(in("A")),bv=l.split(in("B"));
    auto q=l.reg("LOW_OR_QUOTIENT",32,clk,reset,data_we);
    auto r=l.reg("HIGH_OR_REMAINDER",32,clk,reset,data_we);
    auto qv=l.split(q->getOutputPin<32>("Q")),rv=l.split(r->getOutputPin<32>("Q"));
    V nextq,nextr;
    if(!divide) {
        auto ar=l.reg("MULTIPLICAND",32,clk,reset,input_we);
        auto br=l.reg("ORIGINAL_MULTIPLIER",32,clk,reset,input_we);
        auto sa=l.reg("NEGATIVE_A",1,clk,reset,input_we);
        auto sb=l.reg("NEGATIVE_B",1,clk,reset,input_we);
        l.drive(a,ar->getInputPin<32>("D"));l.drive(bv,br->getInputPin<32>("D"));
        l.wire(l.land({in("SIGNED_A"),a[31]}),sa->getInputPin("D"));
        l.wire(l.land({in("SIGNED_B"),bv[31]}),sb->getInputPin("D"));
        auto step=b.add(circuit::families::multiplyStep(32),"STEP");
        l.wire(r->getOutputPin<32>("Q"),step->getInputPin<32>("HI"));
        l.wire(ar->getOutputPin<32>("Q"),step->getInputPin<32>("A"));
        l.wire(qv[0],step->getInputPin("BIT"));
        auto sum=l.split(step->getOutputPin<32>("SUM"));
        nextq=l.shiftRight(qv,sum[0]);nextr=l.shiftRight(sum,step->getOutputPin("CARRY"));
        auto hi=l.sub({rv,l.mux({sa->getOutputPin("Q"),l.constant(32),l.split(br->getOutputPin<32>("Q"))})}).first;
        hi=l.sub({hi,l.mux({sb->getOutputPin("Q"),l.constant(32),l.split(ar->getOutputPin<32>("Q"))})}).first;
        l.drive(hi,p.getOutputPin<32>("HI"));l.drive(qv,p.getOutputPin<32>("LO"));
        l.drive(l.mux({launch,nextq,bv}),q->getInputPin<32>("D"));
    } else {
        auto dr=l.reg("DIVISOR",32,clk,reset,input_we);
        auto ns=l.reg("QUOTIENT_NEGATIVE",1,clk,reset,input_we);
        auto nr=l.reg("REMAINDER_NEGATIVE",1,clk,reset,input_we);
        auto na=l.land({in("SIGNED"),a[31]}),nb=l.land({in("SIGNED"),bv[31]});
        l.drive(l.mux({nb,bv,l.negate(bv)}),dr->getInputPin<32>("D"));
        l.wire(l.lx({na,nb}),ns->getInputPin("D"));l.wire(na,nr->getInputPin("D"));
        auto step=b.add(circuit::families::divideStep(32),"STEP");
        l.wire(r->getOutputPin<32>("Q"),step->getInputPin<32>("REMAINDER"));
        l.wire(dr->getOutputPin<32>("Q"),step->getInputPin<32>("DIVISOR"));
        l.wire(qv[31],step->getInputPin("BIT"));
        nextq=l.shiftLeft(qv,step->getOutputPin("QUOTIENT_BIT"));
        nextr=l.split(step->getOutputPin<32>("NEXT_REMAINDER"));
        auto signedq=l.land({ns->getOutputPin("Q"),l.any(l.split(dr->getOutputPin<32>("Q")))});
        l.drive(l.mux({signedq,qv,l.negate(qv)}),p.getOutputPin<32>("QUOTIENT"));
        l.drive(l.mux({nr->getOutputPin("Q"),rv,l.negate(rv)}),p.getOutputPin<32>("REMAINDER"));
        l.drive(l.mux({launch,nextq,l.mux({na,a,l.negate(a)})}),q->getInputPin<32>("D"));
    }
    l.drive(l.mux({launch,nextr,l.constant(32)}),r->getInputPin<32>("D"));
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
    if(width==4)return small;if(width==32)return word;throw std::invalid_argument("Invalid multiply step width");
}
const ComponentFamily& divideStep(size_t width) {
    static const ComponentFamily small{"arithmetic.divide-step.4","DivideStep<4>",[](IOComponent*p){stepPins(p,4,true);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::DivideStep>(c,n,4);}};
    static const ComponentFamily word{"arithmetic.divide-step.32","DivideStep<32>",[](IOComponent*p){stepPins(p,32,true);},
        [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::DivideStep>(c,n,32);}};
    if(width==4)return small;if(width==32)return word;throw std::invalid_argument("Invalid divide step width");
}
const ComponentFamily Multiplier32{"arithmetic.multiplier.32","Multiplier32",[](IOComponent*p){unitPins(p,false);},structuralFactory<::Multiplier32>()};
const ComponentFamily Divider32{"arithmetic.divider.32","Divider32",[](IOComponent*p){unitPins(p,true);},structuralFactory<::Divider32>()};
const ComponentFamily MulDivUnit32{"rv32im.mul-div.32","MulDivUnit32",[](IOComponent*p){unitPins(p,false,true);},structuralFactory<::MulDivUnit32>(),rv32im::detail::createMulDivBehavioral};
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
Multiplier32::Multiplier32(std::string name):IOComponent(std::move(name),[](IOComponent*p){unitPins(p,false);}){}
void Multiplier32::buildInternals(ComponentBuilder& b){buildUnit(*this,b,false);}
Divider32::Divider32(std::string name):IOComponent(std::move(name),[](IOComponent*p){unitPins(p,true);}){}
void Divider32::buildInternals(ComponentBuilder& b){buildUnit(*this,b,true);}
MulDivUnit32::MulDivUnit32(std::string name):IOComponent(std::move(name),[](IOComponent*p){unitPins(p,false,true);}){}
void MulDivUnit32::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto mul=b.add(circuit::families::Multiplier32,"MULTIPLY");
    auto div=b.add(circuit::families::Divider32,"DIVIDE");
    for(auto s:{"CLK","RST","ENABLE","CANCEL","A","B","TAKE"})for(auto u:{mul,div})l.wire(getInputPinDynamic(s),u->getInputPinDynamic(s));
    auto ready=l.land({mul->getOutputPin("READY"),div->getOutputPin("READY")});
    auto start=l.land({getInputPin("ENABLE"),l.land({getInputPin("START"),ready})});
    auto op=l.split(getInputPin<3>("OP"));
    auto saved=l.reg("OPERATION",3,getInputPin("CLK"),getInputPin("RST"),start);
    l.wire(getInputPin<3>("OP"),saved->getInputPin<3>("D"));
    l.wire(l.land({start,l.inv(op[2])}),mul->getInputPin("START"));l.wire(l.land({start,op[2]}),div->getInputPin("START"));
    l.wire(l.lx({op[0],op[1]}),mul->getInputPin("SIGNED_A"));l.wire(l.land({op[0],l.inv(op[1])}),mul->getInputPin("SIGNED_B"));
    l.wire(l.inv(op[0]),div->getInputPin("SIGNED"));
    auto held=l.split(saved->getOutputPin<3>("Q"));
    auto m=l.mux({l.lor({held[0],held[1]}),l.split(mul->getOutputPin<32>("LO")),l.split(mul->getOutputPin<32>("HI"))});
    auto d=l.mux({held[1],l.split(div->getOutputPin<32>("QUOTIENT")),l.split(div->getOutputPin<32>("REMAINDER"))});
    l.drive(l.mux({held[2],m,d}),getOutputPin<32>("RESULT"));
    l.out(ready,getOutputPin("READY"));
    l.out(l.lor({mul->getOutputPin("BUSY"),div->getOutputPin("BUSY")}),getOutputPin("BUSY"));
    l.out(l.lor({mul->getOutputPin("DONE"),div->getOutputPin("DONE")}),getOutputPin("DONE"));l.finish();
}
namespace rv32im {
void registerArithmetic(circuit::ComponentCatalog& c) {
    for(size_t width:{4u,32u}) {
        detail::registerStructural(c,circuit::families::multiplyStep(width),"MultiplyStepTest");
        detail::registerStructural(c,circuit::families::divideStep(width),"DivideStepTest");
    }
    detail::registerStructural(c,circuit::families::Multiplier32,"Multiplier32Test","clock-boundary");
    detail::registerStructural(c,circuit::families::Divider32,"Divider32Test","clock-boundary");
    detail::registerStructural(c,circuit::families::MulDivUnit32,"MulDivUnit32Test","clock-boundary");
    circuit::ImplementationDescriptor compact;
    compact.id="rv32im.mul-div.32.behavioral";compact.contract_id="rv32im.mul-div.32";
    compact.fidelity=circuit::Fidelity::Behavioral;compact.concrete_type_name="MulDivUnit32";
    compact.supports=[](const circuit::ParameterMap& p){return p.empty();};
    compact.factory=[](const std::string& n,const circuit::ParameterMap&,const std::shared_ptr<circuit::BuildContext>& context){
        return detail::createMulDivBehavioral(n,context);
    };
    compact.evidence={circuit::VerificationStatus::Characterized,
        {"iterative multiplier/divider gates and characterized register state"},{"MulDivUnit32Test"},
        {"MulDivUnit32ScenarioTest","RV32IMFiveStageCoreTest"},{},"four-state","clock-boundary"};
    c.registerImplementation(std::move(compact));
}
}
