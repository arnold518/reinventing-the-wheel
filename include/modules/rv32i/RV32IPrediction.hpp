#pragma once

#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include "rv32i/RV32IPrediction.hpp"

namespace circuit::families {
extern const ComponentFamily RV32IPredictionCounter;
extern const ComponentFamily RV32IBranchPredecode;
extern const ComponentFamily RV32IBranchPredictor;
extern const ComponentFamily RV32IBranchRecovery;
extern const ComponentFamily RV32IPredictionPath;
}

// Policy parameters are construction-time configuration, never fidelity or
// per-instruction controls. Reset initializes dynamic tables to not-taken.
class RV32IPredictionCounter : public IOComponent {
public:
    explicit RV32IPredictionCounter(std::string name);
    static constexpr const char* TypeName = "RV32IPredictionCounter";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

class RV32IBranchPredecode : public IOComponent {
public:
    explicit RV32IBranchPredecode(std::string name);
    static constexpr const char* TypeName = "RV32IBranchPredecode";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

class RV32IBranchRecovery : public IOComponent {
public:
    explicit RV32IBranchRecovery(std::string name);
    static constexpr const char* TypeName = "RV32IBranchRecovery";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

class RV32IBranchPredictor : public IOComponent {
public:
    explicit RV32IBranchPredictor(std::string name, rv32i::PredictionConfig config = {});
    static constexpr const char* TypeName = "RV32IBranchPredictor";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
private:
    rv32i::PredictionConfig config_;
};

class RV32IPredictionPath : public IOComponent {
public:
    explicit RV32IPredictionPath(std::string name, rv32i::PredictionConfig config = {});
    static constexpr const char* TypeName = "RV32IPredictionPath";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
private:
    rv32i::PredictionConfig config_;
};
