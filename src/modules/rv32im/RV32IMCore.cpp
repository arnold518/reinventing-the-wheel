#include "components/ComponentBuilder.tpp"
#include "modules/rv32im/RV32IMCore.hpp"
#include "modules/rv32im/MulDiv.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPipelineStages.hpp"
#include "modules/rv32i/RV32IPipelineObservation.hpp"
#include "modules/rv32i/RV32IExtensionAssembly.hpp"
#include "components/selection/FamilyFactories.hpp"
#include "components/selection/BuildContext.hpp"
#include "StructuralLogic.hpp"
#include "RegisterComponent.hpp"
#include "RV32IMCoreBehavioral.hpp"
#include <map>
#include <set>
using rv32im::detail::Logic;using rv32im::detail::V;using rv32im::detail::P;
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
}
namespace circuit::families {
const ComponentFamily RV32IMDecodeControl{"rv32im.decode-control","RV32IMDecodeControlUnit",controlPins,structuralFactory<::RV32IMDecodeControlUnit>()};
const ComponentFamily RV32IMDecodeStage{"rv32im.pipeline.stage.decode","RV32IMDecodeStage",decodePins,structuralFactory<::RV32IMDecodeStage>()};
const ComponentFamily RV32IMExecuteStage{"rv32im.pipeline.stage.execute","RV32IMExecuteStage",executePins,structuralFactory<::RV32IMExecuteStage>()};
const ComponentFamily RV32IMPipelineCoordinator{"rv32im.pipeline.coordinator","RV32IMPipelineCoordinator",coordinatorPins,structuralFactory<::RV32IMPipelineCoordinator>()};
const ComponentFamily RV32IMFiveStageCore{"rv32im.core.educational-five-stage","RV32IMFiveStageCore",corePins,
    [](const std::string&n,const std::shared_ptr<BuildContext>&c){return Component::createWithContext<::RV32IMFiveStageCore>(c,n,c?rv32i::predictionConfig(c->parameters()):rv32i::PredictionConfig{});},rv32im::detail::createCoreBehavioral};
}
RV32IMDecodeControlUnit::RV32IMDecodeControlUnit(std::string n):IOComponent(std::move(n),controlPins){}
void RV32IMDecodeControlUnit::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto base=b.add(circuit::families::RV32IDecodeControl,"INTEGER");
    l.wire(getInputPin<32>("INSTRUCTION"),base->getInputPin<32>("INSTRUCTION"));
    auto m=isM(l,l.split(getInputPin<32>("INSTRUCTION")));
    const std::map<std::string,uint64_t> values{{"LEGAL",1},{"REG_WRITE",1},{"USES_RS1",1},{"USES_RS2",1},{"WRITEBACK_SEL",1},{"MEM_SIZE",2}};
    for(auto&[n,p]:getAllOutputPins()) {
        auto old=base->getOutputPinDynamic(n);
        if(n=="RS1_ADDR"||n=="RS2_ADDR"||n=="RD_ADDR")l.out(old,p);
        else {
            auto it=values.find(n);auto value=it==values.end()?0:it->second;
            l.drive(l.mux({m,l.split(old),l.constant(p->getWidth(),value)}),p);
        }
    }l.finish();
}
RV32IMDecodeStage::RV32IMDecodeStage(std::string n):IOComponent(std::move(n),decodePins){}
void RV32IMDecodeStage::buildInternals(ComponentBuilder&b){registers_=rv32i::detail::buildDecodeBlueprint(*this,b,circuit::families::RV32IMDecodeControl);}
std::vector<std::vector<LogicValue>> RV32IMDecodeStage::getRegisterStateAtTime(SimulationTime t) const {
    auto v=std::dynamic_pointer_cast<RegisterStateView>(registers_);if(!v)throw std::logic_error("M register observation unavailable");
    return v->getRegisterStateAtTime(t);
}
RV32IMExecuteStage::RV32IMExecuteStage(std::string n):IOComponent(std::move(n),executePins){}
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
    auto word=l.split(getInputPin<32>("IN_INSTRUCTION"));auto m=isM(l,word);
    auto unit=b.add(circuit::families::MulDivUnit32,"MULDIV");
    for(auto n:{"CLK","RST","ENABLE","CANCEL","TAKE"})l.wire(getInputPin(n),unit->getInputPin(n));
    l.wire(forward->getOutputPin<32>("VALUE_A"),unit->getInputPin<32>("A"));
    l.wire(forward->getOutputPin<32>("VALUE_B"),unit->getInputPin<32>("B"));
    l.drive(V(word.begin()+12,word.begin()+15),unit->getInputPin<3>("OP"));
    auto control=l.split(getInputPin<32>("IN_CONTROL"));
    auto fetch_status=l.split(getInputPin<32>("IN_FETCH_STATUS"));
    auto terminal=l.any({control[5],control[6],fetch_status[0],fetch_status[1]});
    auto eligible=l.all({getInputPin("IN_VALID"),m,control[0],l.inv(terminal),l.inv(getInputPin("CANCEL"))});
    // A blocked older producer suppresses WB forwarding too. Do not capture
    // the stale register-file value while that load is still outstanding.
    auto ex_rd=l.split(getInputPin<5>("EX_MEM_RD"));
    auto source_match=l.any({l.equal({V(addresses.begin(),addresses.begin()+5),ex_rd}),
                             l.equal({V(addresses.begin()+5,addresses.begin()+10),ex_rd})});
    auto operand_pending=l.all({getInputPin("EX_MEM_VALID"),getInputPin("EX_MEM_REG_WRITE"),
        l.inv(getInputPin("EX_MEM_RESULT_READY")),l.any(ex_rd),source_match});
    l.wire(l.land({eligible,l.inv(operand_pending)}),unit->getInputPin("START"));
    auto wait=l.land({eligible,l.inv(unit->getOutputPin("DONE"))});
    l.out(wait,getOutputPin("EXECUTION_WAIT"));
    for(auto&[n,p]:base->getAllOutputPins()) {
        if(n=="OUT_ALU_RESULT")l.drive(l.mux({m,l.split(p),l.split(unit->getOutputPin<32>("RESULT"))}),getOutputPin<32>(n));
        else l.out(p,getOutputPinDynamic(n));
    }l.finish();
}
RV32IMPipelineCoordinator::RV32IMPipelineCoordinator(std::string n):IOComponent(std::move(n),coordinatorPins){}
void RV32IMPipelineCoordinator::buildInternals(ComponentBuilder& b) {
    Logic l(b);auto base=b.add(circuit::families::RV32IPipelineCoordinator,"INTEGER");
    for(auto&[n,p]:base->getAllInputPins())l.wire(getInputPinDynamic(n),p);
    auto older=l.any({getInputPin("MEMORY_FAULT"),getInputPin("MEMORY_TERMINAL"),getInputPin("WB_TERMINAL"),
                      getInputPin("HALTED"),getInputPin("TRAPPED")});
    auto hold=l.land({getInputPin("EXECUTION_WAIT"),l.inv(older)});
    const std::set<std::string> movement{"FETCH_VALID","FETCH_PC_WRITE","FETCH_PC_REDIRECT","IF_ID_WRITE","IF_ID_FLUSH","ID_EX_WRITE","ID_EX_FLUSH",
       "EX_MEM_WRITE","EX_MEM_FLUSH","MEM_WB_WRITE","MEM_WB_FLUSH","COMMIT_ENABLE","FETCH_RESPONSE_TAKE","LOAD_USE_STALL","PIPELINE_FLUSH"};
    for(auto&[n,p]:base->getAllOutputPins()) {
        if(n=="PIPELINE_STALL")l.out(l.lor({p,hold}),getOutputPin(n));
        else if(movement.contains(n))l.out(l.land({p,l.inv(hold)}),getOutputPin(n));
        else l.out(p,getOutputPinDynamic(n));
    }
    l.out(hold,getOutputPin("EXECUTION_STALL"));l.finish();
}
RV32IMFiveStageCore::RV32IMFiveStageCore(std::string n,rv32i::PredictionConfig prediction):IOComponent(std::move(n),corePins),prediction_(prediction){prediction_.validate();}
void RV32IMFiveStageCore::buildInternals(ComponentBuilder& b) {
    rv32i::detail::FiveStageFamilies f{circuit::families::RV32IMDecodeStage,circuit::families::RV32IMExecuteStage,circuit::families::RV32IMPipelineCoordinator,
      [](IOComponent&root,ComponentBuilder& builder,circuit::detail::StructuralWiring&w,const rv32i::detail::FiveStageNodes&n){
        for(auto p:{"CLK","RST","ENABLE"})w.connect(root.getInputPin(p),n.execute->getInputPin(p));
        auto cancel=w.reduce("M_CANCEL",{n.memory->getOutputPin("PRETERMINAL"),n.parts.writeback->getOutputPin("TERMINAL"),
             n.parts.status->getOutputPin("HALTED"),n.parts.status->getOutputPin("TRAPPED")},false);
        w.connect(cancel,n.execute->getInputPin("CANCEL"));
        w.connect(n.coordinator->getOutputPin("EX_MEM_WRITE"),n.execute->getInputPin("TAKE"));
        w.connect(n.execute->getOutputPin("EXECUTION_WAIT"),n.coordinator->getInputPin("EXECUTION_WAIT"));
        w.connect(n.coordinator->getOutputPin("EXECUTION_STALL"),root.getOutputPin("EXECUTION_STALL"));
      }};
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
    detail::registerStructural(c,circuit::families::RV32IMPipelineCoordinator,"RV32IMPipelineCoordinatorTest","stable-after-settle",{},{},"known-binary");
    detail::registerStructural(c,circuit::families::RV32IMFiveStageCore,"RV32IMFiveStageCoreTest","instruction-commit",rv32i::supportsPredictionParameters,
        {"rv32i-pipeline-state-view","rv32i-retirement-view"},"known-binary");
    circuit::ImplementationDescriptor compact;
    compact.id="rv32im.core.educational-five-stage.behavioral";
    compact.contract_id="rv32im.core.educational-five-stage";
    compact.fidelity=circuit::Fidelity::Behavioral;compact.concrete_type_name="RV32IMFiveStageCore";
    compact.capabilities={"rv32i-pipeline-state-view","rv32i-retirement-view"};
    compact.supports=rv32i::supportsPredictionParameters;
    compact.factory=[](const std::string& n,const circuit::ParameterMap&,const std::shared_ptr<circuit::BuildContext>& context){
        return detail::createCoreBehavioral(n,context);
    };
    compact.evidence={circuit::VerificationStatus::Characterized,
        {"the five-stage core's own gates and characterized register state"},{"RV32IMFiveStageCoreTest"},
        {"RV32IMFiveStageCoreScenarioTest","RV32IMCompiledProgramTest"},{},"known-binary","instruction-commit"};
    c.registerImplementation(std::move(compact));
}
}
