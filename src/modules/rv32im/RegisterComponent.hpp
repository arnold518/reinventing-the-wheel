#pragma once
#include "components/selection/ComponentCatalog.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "components/IOComponent.hpp"
namespace rv32im::detail {
inline void registerStructural(circuit::ComponentCatalog& catalog,const circuit::ComponentFamily& f,
                               const std::string& test,const std::string& observation="stable-after-settle",
                               circuit::ParameterPredicate supports = {},
                               std::vector<std::string> capabilities = {},
                               const std::string& domain = "four-state") {
    auto schema=Component::create<IOComponent>("SCHEMA",f.pinInitializer());
    std::vector<circuit::PinDescriptor> pins;
    for(auto&[n,p]:schema->getAllInputPins())pins.push_back({n,p->getWidth(),PinType::INPUT});
    for(auto&[n,p]:schema->getAllOutputPins())pins.push_back({n,p->getWidth(),PinType::OUTPUT});
    catalog.registerContract({std::string(f.id()),1,std::string(f.typeName()),pins,capabilities,domain,observation});
    circuit::ImplementationDescriptor d;
    d.id=std::string(f.id())+".structural";d.contract_id=std::string(f.id());
    d.concrete_type_name=std::string(f.typeName());
    d.capabilities=std::move(capabilities);
    d.evidence={circuit::VerificationStatus::Characterized,{"gate/register composition"},{test},{},{},domain,observation};
    d.supports=supports?std::move(supports):circuit::ParameterPredicate([](const circuit::ParameterMap&p){return p.empty();});
    d.factory=[&f](const std::string& n,const circuit::ParameterMap&,const std::shared_ptr<circuit::BuildContext>&c) {
        return f.create(circuit::Fidelity::Structural,n,c);
    };
    catalog.registerImplementation(std::move(d));
}
inline void registerBehavioral(circuit::ComponentCatalog& catalog,const circuit::ComponentFamily& family,
    const std::string& test,const std::string& scenario,circuit::ParameterPredicate supports = {}){
    circuit::ImplementationDescriptor d;
    d.id=std::string(family.id())+".behavioral";d.contract_id=std::string(family.id());
    d.concrete_type_name=std::string(family.typeName());d.fidelity=circuit::Fidelity::Behavioral;
    d.supports=supports?std::move(supports):circuit::ParameterPredicate([](const circuit::ParameterMap& p){return p.empty();});
    d.factory=[&family](const std::string& n,const circuit::ParameterMap&,const std::shared_ptr<circuit::BuildContext>& c){
        return family.create(circuit::Fidelity::Behavioral,n,c);
    };
    d.evidence={circuit::VerificationStatus::Characterized,{"the component's own gates with characterized storage"},
        {test},{scenario},{},"four-state","clock-boundary"};
    catalog.registerImplementation(std::move(d));
}

}
