#include "GateListPartBehavioral.hpp"
#include "analysis/GateListModel.hpp"
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/memory/DigitalMemory.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "basic/WireBase.hpp"
#include "simulator/Event.hpp"
#include <map>
#include <mutex>
#include <stdexcept>

namespace circuit::detail {
namespace {
struct Program {
    std::shared_ptr<const analysis::GateListProgram> gates;
    PartDelays delays;
};
std::shared_ptr<const Program> compiledProgram(const std::string& identity,
    const std::function<std::shared_ptr<IOComponent>()>& reference){
    static std::mutex mutex;
    static std::map<std::string,std::weak_ptr<const Program>> programs;
    const auto key=timing::activeTimingProfile().name+"|"+identity;
    {
        const std::lock_guard<std::mutex> lock(mutex);
        if(auto it=programs.find(key);it!=programs.end())if(auto value=it->second.lock())return value;
    }
    const auto root=reference();
    auto value=std::make_shared<Program>();std::string why;
    value->gates=analysis::GateListModel::compile(root,&why);
    if(!value->gates)throw std::runtime_error(identity+" gate list: "+why);
    analysis::SettlingOptions options;
    if(root->getInputPinDynamic("RST"))options.fixed_inputs["RST"]=0;
    std::vector<std::string> diagnostics;
    value->delays=analysis::CircuitTimingAnalyzer::delays(root,options,"",&diagnostics);
    if(value->delays.empty())throw std::runtime_error(identity+" gate delays: "+
        (diagnostics.empty()?std::string("none"):diagnostics.front()));
    value->delays.evidence="proven at build time on "+identity+" structural gates with characterized storage";
    const std::lock_guard<std::mutex> lock(mutex);
    if(programs.size()>=128)programs.erase(programs.begin());
    programs[key]=value;return value;
}
std::shared_ptr<const Program> program(const ComponentFamily& family,const ParameterMap& parameters){
    return compiledProgram(std::string(family.id())+"|"+canonicalizeParameters(parameters),[&]{
      std::vector<ProfileRule> rules{
        preferFidelity(Fidelity::Behavioral,ProfileSelector::contract("storage.state-bit"),"characterized state bit"),
        preferFidelity(Fidelity::Behavioral,ProfileSelector::contract(families::FlipFlopCell),"characterized enabled bit")};
    for(unsigned width:{1u,2u,3u,4u,5u,8u,16u,32u})
        rules.push_back(preferFidelity(Fidelity::Behavioral,
            ProfileSelector::contract("storage.register."+std::to_string(width)),"characterized register"));
    const auto profile=withProfileOverrides(maximallyStructural(),std::move(rules),"gate-list-reference-v1");
      return std::dynamic_pointer_cast<IOComponent>(builtinComponentCatalog().createRoot(family.request("GATES",parameters),profile).root);
    });
}
class GateListPartBehavioral : public memory_device::Primitive {
public:
    GateListPartBehavioral(std::string name,const ComponentFamily& family,const ParameterMap& parameters)
        : Primitive(std::move(name),std::string(family.typeName()),family.pinInitializer()),
          program_(program(family,parameters)),model_(program_->gates){}
    GateListPartBehavioral(std::string name,std::string type,PinInitFunction pins,
        std::shared_ptr<const Program> program)
        :Primitive(std::move(name),std::move(type),std::move(pins)),
          program_(std::move(program)),model_(program_->gates){}
    void attachDelays(){setDelays(program_->delays);}
    void evaluate(SimulationTime time,Simulator& sim) override {
        if(inputs_.empty()){
            for(const auto&[name,width]:model_.inputs())inputs_.push_back(getInputPinDynamic(name));
            values_.resize(inputs_.size());
        }
        for(size_t i=0;i<inputs_.size();++i)values_[i]=inputs_[i]->getValueAsVector();
        model_.step(values_);
        for(size_t i=0;i<model_.outputs().size();++i){
            const auto& name=model_.outputs()[i].first;
            if(model_.passesClock(i)){
                // The structural bus bridge distributes the ideal clock with
                // zero delay. Other outputs use the proven publication table.
                const auto pin=getOutputPinDynamic(name);const auto clock=read("CLK");
                pin->setValueFromVector(clock);
                if(auto wire=pin->getExternalWireBase())sim.scheduleEvent(makeWireUpdateEvent(time,wire,clock));
            }else publish(sim,time,name,model_.output(i));
        }
    }
private:
    std::shared_ptr<const Program> program_;
    analysis::GateListModel model_;
    std::vector<std::shared_ptr<PinBase>> inputs_;
    std::vector<std::vector<LogicValue>> values_;
};
}
std::shared_ptr<Component> createGateListPart(const ComponentFamily& family,const std::string& name,
    const std::shared_ptr<BuildContext>& context){
    auto part=Component::createWithContext<GateListPartBehavioral>(context,name,family,
        context?context->parameters():ParameterMap{});
    part->attachDelays();return part;
}
std::shared_ptr<IOComponent> createGateListControl(const std::string& name,
    const std::string& type,IOComponent::PinInitFunction pins,const std::string& identity,
    std::function<std::shared_ptr<IOComponent>()> reference,
    const std::shared_ptr<BuildContext>& context){
    auto part=Component::createWithContext<GateListPartBehavioral>(context,name,type,
        std::move(pins),compiledProgram(identity,reference));
    part->attachDelays();return part;
}
}
