#include "components/ComponentBuilder.tpp"
#include "modules/rv32im/RV32IMCore.hpp"
#include "modules/rv32im/MulDiv.hpp"
#include "modules/composite/ArrayMultiplier.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPipelineStages.hpp"
#include "modules/rv32i/RV32IPipelineObservation.hpp"
#include "modules/rv32i/RV32IExtensionAssembly.hpp"
#include "components/selection/FamilyFactories.hpp"
#include "components/selection/BuildContext.hpp"
#include "components/StructuralLogic.hpp"
#include "RegisterComponent.hpp"
#include "RV32IMCoreBehavioral.hpp"
#include "modules/basic/WordMux.hpp"
#include <map>
#include <set>
using circuit::structural::Logic;using circuit::structural::V;using circuit::structural::P;
namespace {
P isM(Logic& l,const V& bits) {
    V matches;constexpr uint32_t mask=0xfe00007f,pattern=0x02000033;
    for(size_t i=0;i<32;++i)if((mask>>i)&1)matches.push_back((pattern>>i)&1?bits[i]:l.inv(bits[i]));
    return l.all(matches);
}
void controlPins(IOComponent*p){circuit::families::RV32IDecodeControl.pinInitializer()(p);}
void decodePins(IOComponent*p){circuit::families::RV32IDecodeStage.pinInitializer()(p);}
void executePins(IOComponent*p){
    circuit::families::RV32IExecuteStage.pinInitializer()(p);
    for(auto s:{"CLK","RST","ENABLE","CANCEL","TAKE"})p->addPin(s,PinType::INPUT);
    p->addPin("EXECUTION_WAIT",PinType::OUTPUT);
}
void coordinatorPins(IOComponent*p){
    circuit::families::RV32IPipelineCoordinator.pinInitializer()(p);
    p->addPin("EXECUTION_WAIT",PinType::INPUT);p->addPin("EXECUTION_STALL",PinType::OUTPUT);
}
void corePins(IOComponent*p){
    circuit::families::RV32IFiveStageCore.pinInitializer()(p);p->addPin("EXECUTION_STALL",PinType::OUTPUT);
}
using Fields=std::vector<std::pair<std::string,size_t>>;
Fields outputFields(const std::shared_ptr<IOComponent>& part) {
    Fields fields;
    for(const auto&[name,pin]:part->getAllOutputPins())fields.emplace_back(name,pin->getWidth());
    return fields;
}
class ExtensionGroup : public IOComponent {
    std::string type_;
    std::function<void(IOComponent&,ComponentBuilder&)> build_;
public:
    ExtensionGroup(std::string name,std::string type,std::function<void(IOComponent*)> pins,
                   std::function<void(IOComponent&,ComponentBuilder&)> build)
        : IOComponent(std::move(name),std::move(pins)),type_(std::move(type)),build_(std::move(build)) {}
    const char* getTypeName()const override{return type_.c_str();}
    void buildInternals(ComponentBuilder& b)override{build_(*this,b);}
};

// As in the I execute stage, packet/ownership decisions are one inspectable
// structural block. The original forwarding and arithmetic families remain.
class MExecutionControl : public IOComponent {
public:
    explicit MExecutionControl(std::string name):IOComponent(std::move(name),[](IOComponent* p) {
        for(auto n:{"IN_VALID","CANCEL","DONE","EX_MEM_VALID","EX_MEM_REG_WRITE","EX_MEM_RESULT_READY"})p->addPin(n,PinType::INPUT);
        for(auto n:{"IN_INSTRUCTION","IN_CONTROL","IN_FETCH_STATUS","IN_REGISTER_ADDRESSES"})p->addPin<32>(n,PinType::INPUT);
        p->addPin<5>("EX_MEM_RD",PinType::INPUT);
        for(auto n:{"IS_M","START","WAIT"})p->addPin(n,PinType::OUTPUT);
        p->addPin<3>("OP",PinType::OUTPUT);
    }) {}
    const char* getTypeName()const override{return "RV32IMExecutionControl";}
    void buildInternals(ComponentBuilder& b)override {
        Logic l(b); auto word=l.split(getInputPin<32>("IN_INSTRUCTION")),addresses=l.split(getInputPin<32>("IN_REGISTER_ADDRESSES"));
        auto m=isM(l,word);
        auto control=l.split(getInputPin<32>("IN_CONTROL")),status=l.split(getInputPin<32>("IN_FETCH_STATUS"));
        auto terminal=l.any({control[5],control[6],status[0],status[1]});
        auto eligible=l.all({getInputPin("IN_VALID"),m,control[0],l.inv(terminal),l.inv(getInputPin("CANCEL"))});
        auto rd=l.split(getInputPin<5>("EX_MEM_RD"));
        auto match=l.any({l.equal({V(addresses.begin(),addresses.begin()+5),rd}),l.equal({V(addresses.begin()+5,addresses.begin()+10),rd})});
        auto pending=l.all({getInputPin("EX_MEM_VALID"),getInputPin("EX_MEM_REG_WRITE"),l.inv(getInputPin("EX_MEM_RESULT_READY")),l.any(rd),match});
        l.out(m,getOutputPin("IS_M")); l.drive(V(word.begin()+12,word.begin()+15),getOutputPin<3>("OP"));
        l.out(l.land({eligible,l.inv(pending)}),getOutputPin("START"));
        l.out(l.land({eligible,l.inv(getInputPin("DONE"))}),getOutputPin("WAIT")); l.finish();
    }
};
}
namespace circuit::families {
const ComponentFamily RV32IMDecodeControl{"rv32im.decode-control","RV32IMDecodeControlUnit",controlPins,structuralFactory<::RV32IMDecodeControlUnit>()};
const ComponentFamily RV32IMDecodeStage{"rv32im.pipeline.stage.decode","RV32IMDecodeStage",decodePins,structuralFactory<::RV32IMDecodeStage>()};
const ComponentFamily RV32IMExecuteStage{"rv32im.pipeline.stage.execute","RV32IMExecuteStage",executePins,structuralFactory<::RV32IMExecuteStage>()};
// The same stage with the array multiplier (core parameter multiplier=array).
const ComponentFamily RV32IMExecuteStageArrayMultiplier{"rv32im.pipeline.stage.execute-array-multiplier","RV32IMExecuteStage",executePins,
    [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::RV32IMExecuteStage>(c,n,true);}};
const ComponentFamily RV32IMPipelineCoordinator{"rv32im.pipeline.coordinator","RV32IMPipelineCoordinator",coordinatorPins,structuralFactory<::RV32IMPipelineCoordinator>()};
const ComponentFamily RV32IMFiveStageCore{"rv32im.core.educational-five-stage","RV32IMFiveStageCore",corePins,
    [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::RV32IMFiveStageCore>(c,n,c?rv32i::predictionConfig(rv32im::predictionParameters(c->parameters())):rv32i::PredictionConfig{});},rv32im::detail::createCoreBehavioral};
}
namespace rv32im {
// The core's parameters: the branch predictor's, plus multiplier=iterative|array.
circuit::ParameterMap predictionParameters(circuit::ParameterMap parameters){parameters.erase("multiplier");parameters.erase("overlapped_fetch");return parameters;}
bool overlappedFetch(const circuit::ParameterMap& parameters){
    auto it=parameters.find("overlapped_fetch");
    if(it==parameters.end()||it->second=="0")return false;
    if(it->second=="1")return true;
    throw std::invalid_argument("overlapped_fetch must be 0 or 1");
}
bool arrayMultiplier(const circuit::ParameterMap& parameters){
    auto it=parameters.find("multiplier");
    if(it==parameters.end()||it->second=="iterative")return false;
    if(it->second=="array")return true;
    throw std::invalid_argument("multiplier must be iterative or array");
}
bool supportsCoreParameters(const circuit::ParameterMap& parameters){
    try{(void)arrayMultiplier(parameters);(void)overlappedFetch(parameters);}catch(const std::exception&){return false;}
    return rv32i::supportsPredictionParameters(predictionParameters(parameters));
}
}
RV32IMDecodeControlUnit::RV32IMDecodeControlUnit(std::string n):IOComponent(std::move(n),controlPins){}
void RV32IMDecodeControlUnit::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto base=b.add(circuit::families::RV32IDecodeControl,"INTEGER");
    l.wire(getInputPin<32>("INSTRUCTION"),base->getInputPin<32>("INSTRUCTION"));
    auto extension=b.addNewComponent<ExtensionGroup>("EXTENSION","RV32IMDecodeOverrides",
        [fields=outputFields(base)](IOComponent* p) {
            p->addPin<32>("INSTRUCTION",PinType::INPUT);
            for(const auto&[name,width]:fields){p->addPinDynamic("INTEGER_"+name,PinType::INPUT,width);p->addPinDynamic(name,PinType::OUTPUT,width);}
        },[](IOComponent& self,ComponentBuilder& builder) {
            Logic logic(builder);auto m=isM(logic,logic.split(self.getInputPin<32>("INSTRUCTION")));
            const std::map<std::string,uint64_t> values{{"LEGAL",1},{"REG_WRITE",1},{"USES_RS1",1},{"USES_RS2",1},{"WRITEBACK_SEL",1},{"MEM_SIZE",2}};
            for(auto&[name,pin]:self.getAllOutputPins()) {
                auto old=self.getInputPinDynamic("INTEGER_"+name);
                if(name=="RS1_ADDR"||name=="RS2_ADDR"||name=="RD_ADDR")logic.out(old,pin);
                else {
                    auto found=values.find(name);auto value=found==values.end()?0:found->second;
                    logic.drive(logic.mux({m,logic.split(old),logic.constant(pin->getWidth(),value)}),pin);
                }
            }logic.finish();
        });
    l.wire(getInputPin<32>("INSTRUCTION"),extension->getInputPin<32>("INSTRUCTION"));
    for(auto&[name,pin]:base->getAllOutputPins())l.wire(pin,extension->getInputPinDynamic("INTEGER_"+name));
    for(auto&[name,pin]:extension->getAllOutputPins())l.wire(pin,getOutputPinDynamic(name));
    l.finish();
}
RV32IMDecodeStage::RV32IMDecodeStage(std::string n):IOComponent(std::move(n),decodePins){}
void RV32IMDecodeStage::buildInternals(ComponentBuilder&b){registers_=rv32i::detail::buildDecodeBlueprint(*this,b,circuit::families::RV32IMDecodeControl);}
std::vector<std::vector<LogicValue>> RV32IMDecodeStage::getRegisterStateAtTime(SimulationTime t) const {
    auto v=std::dynamic_pointer_cast<RegisterStateView>(registers_);if(!v)throw std::logic_error("M register observation unavailable");
    return v->getRegisterStateAtTime(t);
}
RV32IMExecuteStage::RV32IMExecuteStage(std::string n,bool array_multiplier):IOComponent(std::move(n),executePins),array_multiplier_(array_multiplier){}
void RV32IMExecuteStage::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto base=b.add(circuit::families::RV32IExecuteStage,"INTEGER");
    for(auto&[n,p]:base->getAllInputPins())l.wire(getInputPinDynamic(n),p);
    auto forward=rv32i::detail::addForwardedOperands(b,"M_OPERANDS");
    for(auto n:{"EX_MEM_VALID","EX_MEM_RD","EX_MEM_REG_WRITE","EX_MEM_RESULT_READY","EX_MEM_VALUE","MEM_WB_VALID","MEM_WB_RD","MEM_WB_REG_WRITE","MEM_WB_VALUE"})
        l.wire(getInputPinDynamic(n),forward->getInputPinDynamic(n));
    l.wire(getInputPin<32>("IN_RS1_VALUE"),forward->getInputPin<32>("RS1_VALUE"));
    l.wire(getInputPin<32>("IN_RS2_VALUE"),forward->getInputPin<32>("RS2_VALUE"));
    auto addresses=l.split(getInputPin<32>("IN_REGISTER_ADDRESSES"));
    l.drive(V(addresses.begin(),addresses.begin()+5),forward->getInputPin<5>("ID_EX_RS1"));
    l.drive(V(addresses.begin()+5,addresses.begin()+10),forward->getInputPin<5>("ID_EX_RS2"));
    auto control=b.addNewComponent<MExecutionControl>("M_CONTROL");
    for(auto&[n,pin]:control->getAllInputPins())if(n!="DONE")l.wire(getInputPinDynamic(n),pin);
    auto unit=b.add(array_multiplier_?circuit::families::arrayMulDivUnit(32):circuit::families::mulDivUnit(32),"MULDIV");
    for(auto n:{"CLK","RST","ENABLE","CANCEL","TAKE"})l.wire(getInputPin(n),unit->getInputPin(n));
    l.wire(forward->getOutputPin<32>("VALUE_A"),unit->getInputPin<32>("A"));
    l.wire(forward->getOutputPin<32>("VALUE_B"),unit->getInputPin<32>("B"));
    l.wire(control->getOutputPin<3>("OP"),unit->getInputPin<3>("OP"));
    l.wire(unit->getOutputPin("DONE"),control->getInputPin("DONE"));
    l.wire(control->getOutputPin("START"),unit->getInputPin("START"));
    l.wire(control->getOutputPin("WAIT"),getOutputPin("EXECUTION_WAIT"));
    auto select=b.add(circuit::word::mux(2,32),"RESULT_SELECTION");
    l.wire(control->getOutputPin("IS_M"),select->getInputPin("SEL"));
    l.wire(base->getOutputPin<32>("OUT_ALU_RESULT"),select->getInputPin<32>("A"));
    l.wire(unit->getOutputPin<32>("RESULT"),select->getInputPin<32>("B"));
    for(auto&[n,p]:base->getAllOutputPins()) {
        if(n=="OUT_ALU_RESULT")l.wire(select->getOutputPin<32>("OUT"),getOutputPin<32>(n));
        else l.wire(p,getOutputPinDynamic(n));
    }l.finish();
}
RV32IMPipelineCoordinator::RV32IMPipelineCoordinator(std::string n):IOComponent(std::move(n),coordinatorPins){}
void RV32IMPipelineCoordinator::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto base=b.add(circuit::families::RV32IPipelineCoordinator,"INTEGER");
    for(auto&[n,p]:base->getAllInputPins())l.wire(getInputPinDynamic(n),p);
    auto extension=b.addNewComponent<ExtensionGroup>("ARITHMETIC_HOLD","RV32IMPipelineHoldControl",
        [fields=outputFields(base)](IOComponent* p) {
            coordinatorPins(p);
            for(const auto&[name,width]:fields)p->addPinDynamic("INTEGER_"+name,PinType::INPUT,width);
        },[](IOComponent& self,ComponentBuilder& builder) {
            Logic logic(builder);
            auto older=logic.any({self.getInputPin("MEMORY_FAULT"),self.getInputPin("MEMORY_TERMINAL"),self.getInputPin("WB_TERMINAL"),self.getInputPin("HALTED"),self.getInputPin("TRAPPED")});
            auto hold=logic.land({self.getInputPin("EXECUTION_WAIT"),logic.inv(older)});
            const std::set<std::string> movement{"FETCH_VALID","FETCH_PC_WRITE","FETCH_PC_REDIRECT","IF_ID_WRITE","IF_ID_FLUSH","ID_EX_WRITE","ID_EX_FLUSH",
                "EX_MEM_WRITE","EX_MEM_FLUSH","MEM_WB_WRITE","MEM_WB_FLUSH","COMMIT_ENABLE","FETCH_RESPONSE_TAKE","LOAD_USE_STALL","PIPELINE_FLUSH"};
            for(auto&[name,pin]:self.getAllOutputPins()) {
                if(name=="EXECUTION_STALL"){logic.out(hold,pin);continue;}
                auto old=self.getInputPinDynamic("INTEGER_"+name);
                if(name=="PIPELINE_STALL")logic.out(logic.lor({old,hold}),pin);
                else if(movement.contains(name))logic.out(logic.land({old,logic.inv(hold)}),pin);
                else logic.out(old,pin);
            }logic.finish();
        });
    for(auto&[name,pin]:extension->getAllInputPins())
        l.wire(name.starts_with("INTEGER_")?base->getOutputPinDynamic(name.substr(8)):getInputPinDynamic(name),pin);
    for(auto&[name,pin]:extension->getAllOutputPins())l.wire(pin,getOutputPinDynamic(name));
    l.finish();
}
RV32IMFiveStageCore::RV32IMFiveStageCore(std::string n,rv32i::PredictionConfig prediction):IOComponent(std::move(n),corePins),prediction_(prediction){prediction_.validate();}
void RV32IMFiveStageCore::buildInternals(ComponentBuilder& b) {
    const bool array=b.getBuildContext()&&rv32im::arrayMultiplier(b.getBuildContext()->parameters());
    rv32i::detail::FiveStageFamilies f{circuit::families::RV32IMDecodeStage,
      array?circuit::families::RV32IMExecuteStageArrayMultiplier:circuit::families::RV32IMExecuteStage,circuit::families::RV32IMPipelineCoordinator,
      [](IOComponent&root,ComponentBuilder& builder,circuit::detail::StructuralWiring&w,const rv32i::detail::FiveStageNodes&n){
        for(auto p:{"CLK","RST","ENABLE"})w.connect(root.getInputPin(p),n.execute->getInputPin(p));
        auto cancel=w.reduce("M_CANCEL",{n.memory->getOutputPin("PRETERMINAL"),n.parts.writeback->getOutputPin("TERMINAL"),
             n.parts.status->getOutputPin("HALTED"),n.parts.status->getOutputPin("TRAPPED")},false);
        w.connect(cancel,n.execute->getInputPin("CANCEL"));
        w.connect(n.coordinator->getOutputPin("EX_MEM_WRITE"),n.execute->getInputPin("TAKE"));
        w.connect(n.execute->getOutputPin("EXECUTION_WAIT"),n.coordinator->getInputPin("EXECUTION_WAIT"));
        w.connect(n.coordinator->getOutputPin("EXECUTION_STALL"),root.getOutputPin("EXECUTION_STALL"));
      }};
    f.overlapped_fetch=b.getBuildContext()&&rv32im::overlappedFetch(b.getBuildContext()->parameters());
    auto p=rv32i::detail::buildFiveStageBlueprint(*this,b,prediction_,f);
    retirement_=p.retirement;decode_=p.decode;writeback_=p.writeback;status_=p.status;
}
rv32i::RV32IArchitecturalState RV32IMFiveStageCore::snapshotPipelineState(uint32_t pc) const {
    return rv32i::detail::snapshotFiveStage({retirement_,decode_,writeback_,status_},pc);
}
RV32IRetirementObservation RV32IMFiveStageCore::pendingRetirement() const {
    return rv32i::observeRetirementPacket(*retirement_,writeback_->getOutputPin("TERMINAL")->getValue()==LogicValue::HIGH);
}
namespace rv32im {
void registerCore(circuit::ComponentCatalog&c){
    detail::registerStructural(c,circuit::families::RV32IMDecodeControl,"RV32IMDecodeControlTest","stable-after-settle",{},{},"known-binary");
    detail::registerStructural(c,circuit::families::RV32IMDecodeStage,"RV32IMDecodeStageTest","clock-boundary",{},{"register-state-view"},"known-binary");
    detail::registerStructural(c,circuit::families::RV32IMExecuteStage,"RV32IMExecuteStageTest","clock-boundary",{},{},"known-binary");
    detail::registerStructural(c,circuit::families::RV32IMExecuteStageArrayMultiplier,"ArrayMulDivUnitTest","clock-boundary",{},{},"known-binary");
    detail::registerStructural(c,circuit::families::RV32IMPipelineCoordinator,"RV32IMPipelineCoordinatorTest","stable-after-settle",{},{},"known-binary");
    detail::registerStructural(c,circuit::families::RV32IMFiveStageCore,"RV32IMFiveStageCoreTest","instruction-commit",supportsCoreParameters,
        {"rv32i-pipeline-state-view","rv32i-retirement-view"},"known-binary");
    circuit::ImplementationDescriptor compact;
    compact.id="rv32im.core.educational-five-stage.behavioral";
    compact.contract_id="rv32im.core.educational-five-stage";
    compact.fidelity=circuit::Fidelity::Behavioral;compact.concrete_type_name="RV32IMFiveStageCore";
    compact.capabilities={"rv32i-pipeline-state-view","rv32i-retirement-view"};
    compact.supports=supportsCoreParameters;
    compact.factory=[](const std::string& n,const circuit::ParameterMap&,const std::shared_ptr<circuit::BuildContext>& context){
        return detail::createCoreBehavioral(n,context);
    };
    compact.evidence={circuit::VerificationStatus::Characterized,
        {"the five-stage core's own gates and characterized register state"},{"RV32IMFiveStageCoreTest"},
        {"RV32IMFiveStageCoreScenarioTest","RV32IMCompiledProgramTest"},{},"known-binary","instruction-commit"};
    c.registerImplementation(std::move(compact));
}
}
