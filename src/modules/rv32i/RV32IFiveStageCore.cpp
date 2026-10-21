#include "components/StructuralNetBuilder.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "modules/rv32i/RV32IPrediction.hpp"
#include "components/selection/BuildContext.hpp"
#include "RV32IMemoryRequestWiring.hpp"
#include "RV32IPipelineAssembly.hpp"
#include "RV32IExtensionAssembly.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "modules/rv32i/RV32IFiveStageCoreBehavioral.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IPipelineRegisters.hpp"
#include "modules/rv32i/RV32IPipelineStages.hpp"
#include "modules/rv32i/RV32IPipelineObservation.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
void fiveStageCorePins(IOComponent* self) {
    self->addPin("CLK", PinType::INPUT);
    self->addPin("RST", PinType::INPUT);
    self->addPin("ENABLE", PinType::INPUT);
    rv32i::memoryRequestPins(self, "IMEM_");
    rv32i::memoryRequestPins(self, "DMEM_");

    self->addPin<32>("FETCH_PC", PinType::OUTPUT);
    self->addPin("HALTED", PinType::OUTPUT);
    self->addPin("TRAPPED", PinType::OUTPUT);
    self->addPin<4>("TRAP_CAUSE", PinType::OUTPUT);
    self->addPin("INSTRUCTION_ATTEMPT", PinType::OUTPUT);
    self->addPin("COMMIT_VALID", PinType::OUTPUT);


    self->addPin("PIPELINE_STALL", PinType::OUTPUT);
    self->addPin("LOAD_USE_STALL", PinType::OUTPUT);
    self->addPin("MEMORY_STALL", PinType::OUTPUT);
    self->addPin("DATA_PORT_STALL", PinType::OUTPUT);
    self->addPin("PIPELINE_FLUSH", PinType::OUTPUT);
    for(auto p:{"BRANCH_RESOLVED","BRANCH_MISS","PREDICTION_RECOVERY"}) self->addPin(p,PinType::OUTPUT);
    self->addPin("IF_ID_VALID", PinType::OUTPUT);
    self->addPin<32>("IF_ID_PC", PinType::OUTPUT);
    self->addPin("ID_EX_VALID", PinType::OUTPUT);
    self->addPin<32>("ID_EX_PC", PinType::OUTPUT);
    self->addPin("EX_MEM_VALID", PinType::OUTPUT);
    self->addPin<32>("EX_MEM_PC", PinType::OUTPUT);
    self->addPin("MEM_WB_VALID", PinType::OUTPUT);
    self->addPin<32>("MEM_WB_PC", PinType::OUTPUT);
}
} // namespace

namespace circuit::families {
const ComponentFamily RV32IFiveStageCore{
    "rv32i.core.educational-five-stage",
    "RV32IFiveStageCore",
    fiveStageCorePins,
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<::RV32IFiveStageCore>(
            context, name, context ? rv32i::predictionConfig(context->parameters()) : rv32i::PredictionConfig{});
    },
    [](const std::string& name, const std::shared_ptr<BuildContext>& context) {
        return Component::createWithContext<
            ::RV32IFiveStageCoreBehavioral>(context, name, context ? rv32i::predictionConfig(context->parameters()) : rv32i::PredictionConfig{});
    }};
} // namespace circuit::families

RV32IFiveStageCore::RV32IFiveStageCore(std::string name, rv32i::PredictionConfig prediction)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IFiveStageCore.pinInitializer()), prediction_(prediction) { prediction_.validate(); }

void RV32IFiveStageCore::buildInternals(ComponentBuilder& builder) {
    auto p=rv32i::detail::buildFiveStageBlueprint(*this,builder,prediction_,{
        circuit::families::RV32IDecodeStage,circuit::families::RV32IExecuteStage,
        circuit::families::RV32IPipelineCoordinator,{}});
    retirement_packet_=p.retirement;decode_stage_=p.decode;
    writeback_stage_=p.writeback;execution_status_=p.status;
}

rv32i::RV32IArchitecturalState
RV32IFiveStageCore::snapshotPipelineState(uint32_t observed_pc) const {
    return rv32i::detail::snapshotFiveStage(
        {retirement_packet_,decode_stage_,writeback_stage_,execution_status_},observed_pc);
}

RV32IRetirementObservation RV32IFiveStageCore::pendingRetirement() const {
    return rv32i::observeRetirementPacket(*retirement_packet_, writeback_stage_->getOutputPin("TERMINAL")->getValue()==LogicValue::HIGH);
}
