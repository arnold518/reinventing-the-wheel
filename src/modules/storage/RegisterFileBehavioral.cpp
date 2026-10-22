#include "RegisterFileBehavioral.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>

namespace circuit::storage {
namespace {
std::vector<LogicValue> readWord(std::vector<std::vector<LogicValue>> level,const std::vector<LogicValue>& address) {
    for(size_t stage=0;level.size()>1;++stage) {
        std::vector<std::vector<LogicValue>> next(level.size()/2,std::vector<LogicValue>(level.front().size()));
        for(size_t pair=0;pair<next.size();++pair)for(size_t bit=0;bit<next[pair].size();++bit)
            next[pair][bit]=logic_values::mux2(level[2*pair][bit],level[2*pair+1][bit],address[stage]);
        level=std::move(next);
    }
    return level.front();
}
}
RegisterFileBehavioral::RegisterFileBehavioral(std::string name,size_t width,size_t count,RegisterFileConfig c)
    :BasicComponent(std::move(name),1,registerFile(width,count,c).pinInitializer()),width_(width),config_(c),
     type_(registerFile(width,count,c).typeName()),registers_(count,std::vector<LogicValue>(width,LogicValue::UNKNOWN)) {
    if(c.hardwire_zero)registers_[0].assign(width,LogicValue::LOW);
    record(0);
}
void RegisterFileBehavioral::evaluate(SimulationTime time,Simulator& sim) {
    const auto before=registers_;
    register_values::step(registers_,previous_clock_,getInputValue("RST"),getInputValue("CLK"),getInputValue("REG_WRITE"),
                         getInputPinDynamic("RD_ADDR")->getValueAsVector(),getInputPinDynamic("WRITE_DATA")->getValueAsVector(),config_.hardwire_zero);
    if(registers_!=before) {
        if(!sim.isHistoryRecordingEnabled())history_.clear();else record(time);
    }
    for(auto port:{"RS1","RS2"})
        _updateOutputWireDynamic(sim,std::string(port)+"_DATA",readWord(registers_,getInputPinDynamic(std::string(port)+"_ADDR")->getValueAsVector()),time);
}
void RegisterFileBehavioral::record(SimulationTime time) {
    if(!history_.empty()&&time<history_.back().first)history_.clear();
    if(!history_.empty()&&time==history_.back().first)history_.back().second=registers_;
    else if(history_.empty()||history_.back().second!=registers_)history_.emplace_back(time,registers_);
}
RegisterFileBehavioral::Snapshot RegisterFileBehavioral::getRegisterStateAtTime(SimulationTime time)const {
    if(history_.empty())return registers_;
    auto upper=std::upper_bound(history_.begin(),history_.end(),time,[](auto t,const auto& entry){return t<entry.first;});
    return upper==history_.begin()?history_.front().second:(--upper)->second;
}
}
