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
#define RV32I_PREDICTION_COMPONENT(Name) \
class Name : public IOComponent { \
public: \
    explicit Name(std::string name); \
    static constexpr const char* TypeName = #Name; \
    const char* getTypeName() const override { return TypeName; } \
    void buildInternals(ComponentBuilder&) override; \
};
RV32I_PREDICTION_COMPONENT(RV32IPredictionCounter)
RV32I_PREDICTION_COMPONENT(RV32IBranchPredecode)
RV32I_PREDICTION_COMPONENT(RV32IBranchRecovery)
#undef RV32I_PREDICTION_COMPONENT

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
