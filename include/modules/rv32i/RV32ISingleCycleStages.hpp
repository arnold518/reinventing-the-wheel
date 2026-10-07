#pragma once

#include "components/IOComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "components/selection/ComponentFamily.hpp"

// Logical datapath partitions, NOT pipeline registers or microcoded cycles.
// Fetch owns PC; Decode owns the architectural registers. Only Fetch/Memory
// contain transaction state. All architectural writes use the completion edge.
namespace circuit::families { extern const ComponentFamily RV32ISingleCycleFetchStage; }
class RV32ISingleCycleFetchStage : public IOComponent {
public:
    explicit RV32ISingleCycleFetchStage(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleFetchStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

namespace circuit::families { extern const ComponentFamily RV32ISingleCycleExecuteStage; }
class RV32ISingleCycleExecuteStage : public IOComponent {
public:
    explicit RV32ISingleCycleExecuteStage(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleExecuteStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

namespace circuit::families { extern const ComponentFamily RV32ISingleCycleMemoryStage; }
class RV32ISingleCycleMemoryStage : public IOComponent {
public:
    explicit RV32ISingleCycleMemoryStage(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleMemoryStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

namespace circuit::families { extern const ComponentFamily RV32ISingleCycleWritebackStage; }
class RV32ISingleCycleWritebackStage : public IOComponent {
public:
    explicit RV32ISingleCycleWritebackStage(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleWritebackStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

namespace circuit::families { extern const ComponentFamily RV32ISingleCycleNextPCStage; }
class RV32ISingleCycleNextPCStage : public IOComponent {
public:
    explicit RV32ISingleCycleNextPCStage(std::string name);
    static constexpr const char* TypeName = "RV32ISingleCycleNextPCStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

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
