#pragma once
#include "components/BasicComponent.hpp"
#include "modules/storage/RegisterFile.hpp"
#include "RegisterFileValue.hpp"

namespace circuit::storage {
class RegisterFileBehavioral final : public BasicComponent,public RegisterStateView {
    using Snapshot=std::vector<std::vector<LogicValue>>;
    size_t width_;RegisterFileConfig config_;std::string type_;
    Snapshot registers_;
    std::vector<std::pair<SimulationTime,Snapshot>> history_;
    LogicValue previous_clock_=LogicValue::UNKNOWN;
    void record(SimulationTime);
public:
    RegisterFileBehavioral(std::string,size_t width,size_t count,RegisterFileConfig);
    const char* getTypeName()const override{return type_.c_str();}
    void evaluate(SimulationTime,Simulator&)override;
    Snapshot getRegisterStateAtTime(SimulationTime)const override;
};
}
