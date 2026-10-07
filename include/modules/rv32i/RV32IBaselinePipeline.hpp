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

class RV32IBaselineFetchStage : public IOComponent {
public:
    explicit RV32IBaselineFetchStage(std::string name);
    static constexpr const char* TypeName = "RV32IBaselineFetchStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

class RV32IBaselineExecuteStage : public IOComponent {
public:
    explicit RV32IBaselineExecuteStage(std::string name);
    static constexpr const char* TypeName = "RV32IBaselineExecuteStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

class RV32IBaselineMemoryStage : public IOComponent {
public:
    explicit RV32IBaselineMemoryStage(std::string name);
    static constexpr const char* TypeName = "RV32IBaselineMemoryStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

class RV32IBaselineDependencyUnit : public IOComponent {
public:
    explicit RV32IBaselineDependencyUnit(std::string name);
    static constexpr const char* TypeName = "RV32IBaselineDependencyUnit";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

class RV32IBaselineControl : public IOComponent {
public:
    explicit RV32IBaselineControl(std::string name);
    static constexpr const char* TypeName = "RV32IBaselineControl";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

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
