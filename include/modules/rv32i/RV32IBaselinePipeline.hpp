#pragma once

#include "components/IOComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "components/capabilities/RV32IPipelineStateView.hpp"
#include "components/capabilities/RV32IRetirementView.hpp"
#include "components/selection/ComponentFamily.hpp"

// Stall-only, non-speculative, zero-wait-endpoint pipeline. The wait-capable
// RV32IFiveStageCore remains a separate, more advanced implementation.
namespace circuit::families {
extern const ComponentFamily RV32IBaselineFetchStage;
extern const ComponentFamily RV32IBaselineDecodeStage;
extern const ComponentFamily RV32IBaselineExecuteStage;
extern const ComponentFamily RV32IBaselineMemoryStage;
extern const ComponentFamily RV32IBaselineDependencyUnit;
extern const ComponentFamily RV32IBaselineControl;
extern const ComponentFamily RV32IBaselinePipelineCore;
}

#define BASELINE_BLOCK(Name) \
class RV32IBaseline##Name : public IOComponent { \
public: \
    explicit RV32IBaseline##Name(std::string name); \
    static constexpr const char* TypeName = "RV32IBaseline" #Name; \
    const char* getTypeName() const override { return TypeName; } \
    void buildInternals(ComponentBuilder&) override; \
};
BASELINE_BLOCK(FetchStage)
BASELINE_BLOCK(ExecuteStage)
BASELINE_BLOCK(MemoryStage)
BASELINE_BLOCK(DependencyUnit)
BASELINE_BLOCK(Control)
#undef BASELINE_BLOCK

class RV32IBaselineDecodeStage : public IOComponent, public RegisterStateView {
public:
    explicit RV32IBaselineDecodeStage(std::string name);
    static constexpr const char* TypeName = "RV32IBaselineDecodeStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
    std::vector<std::vector<LogicValue>> getRegisterStateAtTime(size_t time) const override;
private:
    std::shared_ptr<IOComponent> registers_;
};

class RV32IBaselinePipelineCore : public IOComponent, public RV32IPipelineStateView, public RV32IRetirementView {
public:
    explicit RV32IBaselinePipelineCore(std::string name);
    static constexpr const char* TypeName = "RV32IBaselinePipelineCore";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
    rv32i::RV32IArchitecturalState snapshotPipelineState(uint32_t observed_pc) const override;
    RV32IRetirementObservation pendingRetirement() const override;

private:
    std::shared_ptr<IOComponent> packet_;
    std::shared_ptr<IOComponent> decode_, writeback_, execution_status_;
};
