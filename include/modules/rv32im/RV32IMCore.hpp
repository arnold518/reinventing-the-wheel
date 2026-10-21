#pragma once

#include "simulator/SimulationTime.hpp"
#include "modules/rv32i/RV32IFiveStageCore.hpp"
#include "components/capabilities/RegisterStateView.hpp"
namespace circuit { class ComponentCatalog; }
namespace circuit::families {
extern const ComponentFamily RV32IMDecodeControl, RV32IMDecodeStage, RV32IMExecuteStage,
    RV32IMPipelineCoordinator, RV32IMFiveStageCore;
}
class RV32IMDecodeControlUnit : public IOComponent {
public:
    explicit RV32IMDecodeControlUnit(std::string);
    static constexpr const char* TypeName="RV32IMDecodeControlUnit";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class RV32IMDecodeStage : public IOComponent, public RegisterStateView {
    std::shared_ptr<IOComponent> registers_;
public:
    explicit RV32IMDecodeStage(std::string);
    static constexpr const char* TypeName="RV32IMDecodeStage";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
    std::vector<std::vector<LogicValue>> getRegisterStateAtTime(SimulationTime) const override;
};
class RV32IMExecuteStage : public IOComponent {
public:
    explicit RV32IMExecuteStage(std::string);
    static constexpr const char* TypeName="RV32IMExecuteStage";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class RV32IMPipelineCoordinator : public IOComponent {
public:
    explicit RV32IMPipelineCoordinator(std::string);
    static constexpr const char* TypeName="RV32IMPipelineCoordinator";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
};
class RV32IMFiveStageCore : public IOComponent, public RV32IPipelineStateView, public RV32IRetirementView {
    rv32i::PredictionConfig prediction_;
    std::shared_ptr<IOComponent> retirement_,decode_,writeback_,status_;
public:
    explicit RV32IMFiveStageCore(std::string,rv32i::PredictionConfig prediction={});
    static constexpr const char* TypeName="RV32IMFiveStageCore";
    const char* getTypeName() const override{return TypeName;}
    void buildInternals(ComponentBuilder&) override;
    rv32i::RV32IArchitecturalState snapshotPipelineState(uint32_t) const override;
    RV32IRetirementObservation pendingRetirement() const override;
};
namespace rv32im { void registerCore(circuit::ComponentCatalog&); }
