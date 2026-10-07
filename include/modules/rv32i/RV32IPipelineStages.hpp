#pragma once

#include "components/IOComponent.hpp"
#include "components/capabilities/RegisterStateView.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <memory>

namespace circuit::families {
extern const ComponentFamily RV32IFetchStage;
extern const ComponentFamily RV32IDecodeStage;
extern const ComponentFamily RV32IExecuteStage;
extern const ComponentFamily RV32IMemoryStage;
extern const ComponentFamily RV32IWritebackStage;
extern const ComponentFamily RV32IPipelineExecutionStatus;
}

class RV32IFetchStage : public IOComponent {
public:
    explicit RV32IFetchStage(std::string name, bool external_next_pc = false);
    static constexpr const char* TypeName = "RV32IFetchStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
private:
    bool external_next_pc_;
};

class RV32IDecodeStage
    : public IOComponent,
      public RegisterStateView {
public:
    explicit RV32IDecodeStage(std::string name);
    static constexpr const char* TypeName = "RV32IDecodeStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;

    std::vector<std::vector<LogicValue>>
    getRegisterStateAtTime(size_t time) const override;

private:
    std::shared_ptr<IOComponent> register_file_{};
};

class RV32IExecuteStage : public IOComponent {
public:
    explicit RV32IExecuteStage(std::string name);
    static constexpr const char* TypeName = "RV32IExecuteStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class RV32IMemoryStage : public IOComponent {
public:
    explicit RV32IMemoryStage(std::string name);
    static constexpr const char* TypeName = "RV32IMemoryStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class RV32IWritebackStage : public IOComponent {
public:
    explicit RV32IWritebackStage(std::string name);
    static constexpr const char* TypeName = "RV32IWritebackStage";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};

class RV32IPipelineExecutionStatus : public IOComponent {
public:
    explicit RV32IPipelineExecutionStatus(std::string name);
    static constexpr const char* TypeName = "RV32IPipelineExecutionStatus";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder& builder) override;
};
