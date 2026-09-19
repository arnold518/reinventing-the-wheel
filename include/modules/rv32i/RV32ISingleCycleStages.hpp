#pragma once

#include "components/IOComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "components/selection/ComponentFamily.hpp"

// Logical datapath partitions, NOT pipeline registers or microcoded cycles.
// Fetch owns PC; Decode owns the architectural registers. Only Fetch/Memory
// contain transaction state. All architectural writes use the completion edge.
#define DECLARE_SINGLE_CYCLE_STAGE(Name) \
namespace circuit::families { extern const ComponentFamily RV32ISingleCycle##Name##Stage; } \
class RV32ISingleCycle##Name##Stage : public IOComponent { \
public: \
    explicit RV32ISingleCycle##Name##Stage(std::string name); \
    static constexpr const char* TypeName = "RV32ISingleCycle" #Name "Stage"; \
    const char* getTypeName() const override { return TypeName; } \
    void buildInternals(ComponentBuilder&) override; \
};

DECLARE_SINGLE_CYCLE_STAGE(Fetch)
DECLARE_SINGLE_CYCLE_STAGE(Execute)
DECLARE_SINGLE_CYCLE_STAGE(Memory)
DECLARE_SINGLE_CYCLE_STAGE(Writeback)
DECLARE_SINGLE_CYCLE_STAGE(NextPC)
#undef DECLARE_SINGLE_CYCLE_STAGE

namespace circuit::families { extern const ComponentFamily RV32ISingleCycleDecodeStage; }
class RV32ISingleCycleDecodeStage : public IOComponent, public RegisterStateView {
public:
    explicit RV32ISingleCycleDecodeStage(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleDecodeStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
    std::vector<std::vector<LogicValue>> getRegisterStateAtTime(size_t time) const override;
private:
    std::shared_ptr<IOComponent> registers_;
};
