#pragma once
#include "basic/LogicValueHelpers.hpp"
#include <cstddef>

namespace circuit::storage::register_values {
// The same write network for any register-file geometry. Address decoding and
// uncertain write enables follow the gates, including the physical zero policy.
template<class Registers,class Address,class Word>
void step(Registers& registers,LogicValue& previous_clock,LogicValue reset,LogicValue clock,
          LogicValue write,const Address& address,const Word& data,bool hardwire_zero=true) {
    using namespace circuit::logic_values;
    reset=sanitizeBit(reset);clock=sanitizeBit(clock);
    if(reset==LogicValue::HIGH) {
        for(auto& word:registers)for(auto& bit:word)bit=LogicValue::LOW;
    } else if(reset!=LogicValue::LOW) {
        for(auto& word:registers)for(auto& bit:word)if(bit!=LogicValue::LOW)bit=LogicValue::UNKNOWN;
    } else if(previous_clock==LogicValue::LOW && clock==LogicValue::HIGH) {
        for(size_t index=hardwire_zero?1:0;index<registers.size();++index) {
            auto enable=sanitizeBit(write);
            for(size_t bit=0;bit<address.size();++bit)
                enable=andGate(enable,(index>>bit)&1U?sanitizeBit(address[bit]):notGate(address[bit]));
            if(enable==LogicValue::LOW)continue;
            for(size_t bit=0;bit<data.size();++bit)
                registers[index][bit]=enable==LogicValue::HIGH?sanitizeBit(data[bit]):mux2(registers[index][bit],sanitizeBit(data[bit]),enable);
        }
    }
    if(hardwire_zero)for(auto& bit:registers[0])bit=LogicValue::LOW;
    previous_clock=clock;
}
}
