#include "MulDivBehavioral.hpp"
#include "analysis/GateListModel.hpp"
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/memory/DigitalMemory.hpp"
#include "modules/rv32im/MulDiv.hpp"
#include <map>
#include <bit>
#include <mutex>
#include <stdexcept>

namespace rv32im::detail {
namespace {
struct Program {
    std::shared_ptr<const circuit::analysis::GateListProgram> gates;
    circuit::PartDelays delays;
};
std::shared_ptr<const Program> program(size_t width) {
    using namespace circuit;
    static std::mutex mutex;
    static std::map<std::string,std::shared_ptr<const Program>> programs;
    const auto key=timing::activeTimingProfile().name+".width"+std::to_string(width);
    {
        const std::lock_guard<std::mutex> lock(mutex);
        if(auto it=programs.find(key);it!=programs.end())return it->second;
    }
    std::vector<ProfileRule> rules;
    for(unsigned bits:{1u,3u,static_cast<unsigned>(std::countr_zero(width)),static_cast<unsigned>(width)})
        rules.push_back(preferFidelity(Fidelity::Behavioral,
            ProfileSelector::contract("storage.register."+std::to_string(bits)),"proven register state"));
    const auto profile=withProfileOverrides(maximallyStructural(),std::move(rules),"mul-div-gates-v1");
    const auto root=builtinComponentCatalog().createRoot(families::mulDivUnit(width).request("GATES"),profile).root;
    auto result=std::make_shared<Program>();std::string why;
    result->gates=analysis::GateListModel::compile(root,&why);
    if(!result->gates)throw std::runtime_error("MulDiv gate list: "+why);
    analysis::SettlingOptions options;options.fixed_inputs["RST"]=0;
    std::vector<std::string> diagnostics;
    result->delays=analysis::CircuitTimingAnalyzer::delays(root,options,"",&diagnostics);
    if(result->delays.empty())throw std::runtime_error("MulDiv gate delays: "+
        (diagnostics.empty()?std::string("none"):diagnostics.front()));
    result->delays.evidence="proven at build time on the iterative width-parameterized MulDivUnit gates with characterized register state";
    const std::lock_guard<std::mutex> lock(mutex);
    return programs.emplace(key,std::move(result)).first->second;
}
// Executes the tested gates, including both iterative units and their ownership
// registers. It contains no host multiplication/division or instruction oracle.
class MulDivBehavioral : public circuit::memory_device::Primitive {
public:
    explicit MulDivBehavioral(std::string name,size_t width)
        : Primitive(std::move(name),std::string(circuit::families::mulDivUnit(width).typeName()),circuit::families::mulDivUnit(width).pinInitializer()),
          program_(program(width)),model_(program_->gates) {}
    void attachDelays(){setDelays(program_->delays);}
    void evaluate(SimulationTime time,Simulator& sim) override {
        if(inputs_.empty()){
            for(const auto&[name,width]:model_.inputs())inputs_.push_back(getInputPinDynamic(name));
            values_.resize(inputs_.size());
        }
        for(size_t i=0;i<inputs_.size();++i)values_[i]=inputs_[i]->getValueAsVector();
        model_.step(values_);
        for(size_t i=0;i<model_.outputs().size();++i)
            publish(sim,time,model_.outputs()[i].first,model_.output(i));
    }
private:
    std::shared_ptr<const Program> program_;
    circuit::analysis::GateListModel model_;
    std::vector<std::shared_ptr<PinBase>> inputs_;
    std::vector<std::vector<LogicValue>> values_;
};
}
std::shared_ptr<Component> createMulDivBehavioral(const std::string& name,
    const std::shared_ptr<circuit::BuildContext>& context,size_t width){
    auto part=Component::createWithContext<MulDivBehavioral>(context,name,width);
    part->attachDelays();return part;
}
}
