#pragma once
#include "components/IOComponent.hpp"
#include <cstdint>

namespace rv32i {
// Compact reference for the FF/gate requester, used by behavioral CPU variants.
struct MemoryRequestState {
    bool busy=false, sent=false, abandoned=false;
    uint32_t tag=0, address=0, data=0;
    unsigned op=0, mask=0;
    bool discarded(uint32_t current) const { return abandoned || current != tag; }
    bool ready(uint32_t current, bool take, bool reset, bool issue=false) const { return !reset && (busy||issue) && (take || (busy&&discarded(current))); }
    bool done(const IOComponent& c, const std::string& p, uint32_t current, bool issue) const {
        // Derive bypass from current state, never the previous evaluation's
        // published output wire (which has not propagated after an edge yet).
        const bool accepted=(busy||issue)&&!sent
            && c.getInputPin(p+"REQ_READY")->getValue()==LogicValue::HIGH;
        return c.getInputPin("RST")->getValue()==LogicValue::LOW && (sent||accepted) && !(busy&&discarded(current))
            && c.getInputPin(p+"RSP_VALID")->getValue()==LogicValue::HIGH;
    }
    void tick(const IOComponent& c, const std::string& p, bool reset, bool edge,
              bool issue, bool take, uint32_t current, unsigned operation, uint32_t addr, uint32_t value, unsigned enables) {
        if (reset) { *this={}; return; }
        if (!edge) return;
        const bool finish=ready(current,take,false,issue) && c.getInputPin(p+"RSP_VALID")->getValue()==LogicValue::HIGH;
        const bool accept=(busy||issue)&&!sent&&c.getInputPin(p+"REQ_READY")->getValue()==LogicValue::HIGH;
        if (finish) { busy=sent=abandoned=false; return; }
        if (!busy) {
            if (issue) { busy=true; sent=accept; tag=current; address=addr; data=value; op=operation; mask=enables; }
        } else {
            abandoned=discarded(current);
            if (!sent && c.getInputPin(p+"REQ_READY")->getValue()==LogicValue::HIGH) sent=true;
        }
    }
};
template<class Emit>
void publishMemoryRequest(const MemoryRequestState& s, bool reset, bool take, uint32_t tag,
    bool issue,unsigned op,uint32_t address,uint32_t data,unsigned mask,Emit emit) {
    emit.template operator()<1>("REQ_VALID", !reset && (s.busy||issue) && !s.sent);
    emit.template operator()<1>("RSP_READY", s.ready(tag,take,reset,issue));
    emit.template operator()<3>("REQ_OP", s.busy?s.op:op);
    emit.template operator()<32>("REQ_ADDR", s.busy?s.address:address);
    emit.template operator()<32>("REQ_WRITE_DATA", s.busy?s.data:data);
    emit.template operator()<4>("REQ_WRITE_MASK", s.busy?s.mask:mask);
}
inline uint32_t memoryWordAddress(uint32_t address, unsigned size) {
    bool bad=size>2 || (size==1 && (address&1)) || (size==2 && (address&3));
    return (address&~uint32_t{3}) | unsigned(bad);
}
inline unsigned memoryWriteMask(uint32_t address,unsigned size) {
    return size==0 ? 1u<<(address&3) : size==1 ? 3u<<(address&2) : size==2 ? 15 : 0;
}
inline uint32_t memoryLoadData(uint32_t word,uint32_t address,unsigned size,bool sign) {
    word >>= (address&3)*8;
    if(size==0) return sign ? uint32_t(int32_t(int8_t(word))) : word&255;
    if(size==1) return sign ? uint32_t(int32_t(int16_t(word))) : word&65535;
    return size==2 ? word : 0;
}
}
