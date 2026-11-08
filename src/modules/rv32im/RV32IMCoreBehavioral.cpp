#include "RV32IMCoreBehavioral.hpp"
#include "analysis/GateListModel.hpp"
#include "analysis/CircuitTimingAnalyzer.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuildContext.hpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/memory/DigitalMemory.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/rv32im/RV32IMCore.hpp"
#include "modules/rv32i/RV32IPipelineObservation.hpp"
#include <map>
#include <mutex>
#include <stdexcept>

namespace rv32im::detail {
namespace {
std::shared_ptr<IOComponent> child(const std::shared_ptr<IOComponent>& root,const std::string& name){
    for(const auto& node:root->getChildren())if(node->getName()==name)
        return std::dynamic_pointer_cast<IOComponent>(node);
    throw std::logic_error("Missing core gate component: "+name);
}
struct Program {
    std::shared_ptr<const circuit::analysis::GateListProgram> gates;
    circuit::PartDelays delays;
};
std::shared_ptr<const Program> program(const circuit::ParameterMap& parameters){
    using namespace circuit;
    static std::mutex mutex;
    static std::map<std::string,std::weak_ptr<const Program>> programs;
    const auto key=timing::activeTimingProfile().name+"|"+canonicalizeParameters(parameters);
    {
        const std::lock_guard<std::mutex> lock(mutex);
        if(auto result=programs[key].lock())return result;
    }
    std::vector<ProfileRule> rules{preferFidelity(Fidelity::Behavioral,
        ProfileSelector::contract("storage.state-bit"),"proven register state")};
    rules.push_back(preferFidelity(Fidelity::Behavioral,
        ProfileSelector::contract(families::FlipFlopCell),"proven enabled bit state"));
    for(unsigned width:{1u,2u,3u,4u,5u,8u,16u,32u})
        rules.push_back(preferFidelity(Fidelity::Behavioral,
            ProfileSelector::contract("storage.register."+std::to_string(width)),"proven register state"));
    const auto profile=withProfileOverrides(maximallyStructural(),std::move(rules),"rv32im-core-gates-v1");
    const auto root=std::dynamic_pointer_cast<IOComponent>(builtinComponentCatalog().createRoot(
        families::RV32IMFiveStageCore.request("GATES",parameters),profile).root);
    const auto storage=child(child(child(root,"DECODE"),"REGISTER_FILE"),"STORAGE");
    std::vector<std::shared_ptr<PinBase>> observed;
    for(unsigned word=0;word<31;++word)observed.push_back(storage->getOutputPinDynamic("WORD_"+std::to_string(word)));
    const auto packet=child(root,"MEM_WB");
    for(const auto* name:{"Q_PC","Q_INSTRUCTION","Q_CONTROL","Q_ALU_RESULT","Q_STORE_DATA",
                         "Q_MEMORY_DATA","Q_MEMORY_STATUS","Q_NEXT_PC"})observed.push_back(packet->getOutputPinDynamic(name));
    observed.push_back(child(root,"WRITEBACK")->getOutputPin("TERMINAL"));
    for(const auto* name:{"HALTED","TRAPPED","TRAP_CAUSE"})observed.push_back(root->getOutputPinDynamic(name));
    auto result=std::make_shared<Program>();std::string why;
    result->gates=analysis::GateListModel::compile(root,&why,observed);
    if(!result->gates)throw std::runtime_error("RV32IM core gate list: "+why);
    analysis::SettlingOptions options;options.fixed_inputs["RST"]=0;
    std::vector<std::string> diagnostics;
    result->delays=analysis::CircuitTimingAnalyzer::delays(root,options,"",&diagnostics);
    if(result->delays.empty())throw std::runtime_error("RV32IM core gate delays: "+
        (diagnostics.empty()?std::string("none"):diagnostics.front()));
    result->delays.evidence="proven at build time on RV32IMFiveStageCore gates with characterized register state";
    const std::lock_guard<std::mutex> lock(mutex);
    if(programs.size()>=128)programs.erase(programs.begin());
    programs[key]=result;return result;
}
class CoreBehavioral : public circuit::memory_device::Primitive,
    public RV32IPipelineStateView, public RV32IRetirementView {
public:
    CoreBehavioral(std::string name,const circuit::ParameterMap& parameters)
        : Primitive(std::move(name),"RV32IMFiveStageCore",circuit::families::RV32IMFiveStageCore.pinInitializer()),
          program_(program(parameters)),model_(program_->gates){}
    void attachDelays(){setDelays(program_->delays);}
    // Saving a machine: what was last published and the gate list's state
    // (every node, the characterized registers inside it); the pin and
    // value caches are rebuilt on each evaluation.
    StateSupport stateSupport() const override {return StateSupport::Saved;}
    void saveOwnState(circuit::state::Writer& out) const override {savePublished(out);model_.saveState(out);}
    void restoreOwnState(circuit::state::Reader& in) override {restorePublished(in);model_.restoreState(in);}
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
    rv32i::RV32IArchitecturalState snapshotPipelineState(uint32_t pc) const override {
        rv32i::RV32IArchitecturalState state;
        state.pc=rv32i::RV32IArchitecturalState::fromKnown(rv32i::RV32IState{.pc=pc}).pc;
        state.x[0].assign(32,LogicValue::LOW);
        for(unsigned word=1;word<32;++word)state.x[word]=model_.observation(word-1);
        state.halted=model_.observation(40).at(0);state.trapped=model_.observation(41).at(0);
        state.trap_cause=model_.observation(42);return state;
    }
    RV32IRetirementObservation pendingRetirement() const override {
        return {word(31),word(32),rv32i::observeMemoryRetirement(
            rv32i::pipeline_encoding::unpackControl(word(33)),word(34),word(35),word(36),
            (word(37)&3u)!=0),word(38),word(39)!=0};
    }
private:
    uint32_t word(size_t observation) const {
        uint32_t result=0;const auto bits=model_.observation(observation);
        for(size_t bit=0;bit<bits.size();++bit){
            if(bits[bit]!=LogicValue::LOW&&bits[bit]!=LogicValue::HIGH)
                throw std::logic_error("Unknown RV32IM retirement circuit signal");
            if(bits[bit]==LogicValue::HIGH)result|=uint32_t{1}<<bit;
        }
        return result;
    }
    std::shared_ptr<const Program> program_;
    circuit::analysis::GateListModel model_;
    std::vector<std::shared_ptr<PinBase>> inputs_;
    std::vector<std::vector<LogicValue>> values_;
};
}
std::shared_ptr<Component> createCoreBehavioral(const std::string& name,
    const std::shared_ptr<circuit::BuildContext>& context){
    auto part=Component::createWithContext<CoreBehavioral>(context,name,context?context->parameters():circuit::ParameterMap{});
    part->attachDelays();return part;
}
}
