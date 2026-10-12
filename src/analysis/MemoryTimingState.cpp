#include "MemoryTimingState.hpp"
#include "../modules/memory/FlipFlopCellBehavioral.hpp"
#include "modules/storage/ClockedStateBitBehavioral.hpp"
#include "modules/rv32i/RV32IMemoryRequest.hpp"
#include "modules/basic/Gate.hpp"
#include "modules/utility/Constant.hpp"
#include <array>
#include <set>
#include <stdexcept>
#include <typeinfo>

namespace circuit::analysis::detail {
namespace {
constexpr auto L=LogicValue::LOW, H=LogicValue::HIGH, X=LogicValue::UNKNOWN;
std::shared_ptr<PinBase> source(std::shared_ptr<PinBase> p) {
    std::set<std::shared_ptr<PinBase>> seen;
    while (seen.insert(p).second) {
        if (p->getType()==PinType::OUTPUT && std::dynamic_pointer_cast<BasicComponent>(p->getOwner())) return p;
        auto w=p->getType()==PinType::INPUT?p->getExternalWireBase():p->getInternalWireBase();
        if (!w || !w->getSourcePinBase()) return p;
        p=w->getSourcePinBase();
    }
    throw std::runtime_error("Control boundary cycle");
}
std::shared_ptr<IOComponent> child(const std::shared_ptr<Component>& c,const std::string& name) {
    for(const auto& p:c->getChildren())if(p->getName()==name) {
        if(auto io=std::dynamic_pointer_cast<IOComponent>(p))return io;
        break;
    }
    throw std::runtime_error("Missing control state component");
}
LogicValue inv(LogicValue a) {return a==L?H:a==H?L:X;}
LogicValue both(LogicValue a,LogicValue b) {return a==L||b==L?L:a==H&&b==H?H:X;}
LogicValue either(LogicValue a,LogicValue b) {return a==H||b==H?H:a==L&&b==L?L:X;}
struct Evaluate {
    std::map<std::shared_ptr<PinBase>,LogicValue> values;
    std::set<std::shared_ptr<PinBase>> active;
    LogicValue get(const std::shared_ptr<PinBase>& p) {
        if(auto it=values.find(p);it!=values.end())return it->second;
        if(active.size()>256 || !active.insert(p).second)throw std::runtime_error("Unsupported control feedback");
        LogicValue result;
        const auto c=p->getOwner();
        if(p->getType()==PinType::OUTPUT && typeid(*c)==typeid(ConstantValue<1>))
            result=static_cast<const ConstantValue<1>&>(*c).getConstantValue()&1?H:L;
        else if(p->getType()==PinType::OUTPUT && typeid(*c)==typeid(NOTGate))
            result=inv(get(std::dynamic_pointer_cast<IOComponent>(c)->getInputPin("IN")));
        else if(p->getType()==PinType::OUTPUT && (typeid(*c)==typeid(ANDGate)||typeid(*c)==typeid(ORGate))) {
            auto io=std::dynamic_pointer_cast<IOComponent>(c);
            const auto a=get(io->getInputPin("A")),b=get(io->getInputPin("B"));
            result=typeid(*c)==typeid(ANDGate)?both(a,b):either(a,b);
        } else {
            // Stop at overridden composite inputs (TAKE/ISSUE/RST) before
            // following their external CPU wires: their values are quantified.
            auto w=p->getType()==PinType::INPUT?p->getExternalWireBase():p->getInternalWireBase();
            if(!w||!w->getSourcePinBase())throw std::runtime_error("Unmodeled control input");
            result=get(w->getSourcePinBase());
        }
        active.erase(p);values[p]=result;return result;
    }
};

// The endpoint of a timed memory with its requester. The endpoint's own
// handshake (BehavioralMemoryEndpoint::evaluate, four-state): PENDING, the
// response buffer's FULL, and what the sequencer shows, which changes only at
// a clock edge: idle (ready for a start), working (not ready, no result) or
// done (result shown). How long the device works is left open, so every
// number of working cycles is covered, and so is a refresh starting by itself.
std::optional<MemoryTimingGroup> sequencedStates(const std::shared_ptr<BehavioralMemoryEndpoint>& endpoint,
    const std::shared_ptr<IOComponent>& request,const std::array<std::shared_ptr<IOComponent>,3>& cells,
    const std::shared_ptr<PinBase>& mismatch) {
    enum Sequencer:unsigned{Idle,Working,Done};
    const auto bit=[](bool v){return v?H:L;};
    const auto clean=[](LogicValue v){return v==H||v==L?v:X;};
    struct View{LogicValue ready,valid,accept,finish,full,pending;};
    const auto view=[&](LogicValue pending,LogicValue full,unsigned seq,LogicValue take,LogicValue issue) {
        const auto room=either(inv(full),take);
        const auto complete=both(pending,bit(seq==Done));
        const auto slot=both(either(inv(pending),both(complete,take)),bit(seq!=Working));
        const auto ready=both(room,slot),accept=both(ready,issue),finish=both(complete,room);
        const auto capture=both(both(complete,room),either(full,inv(take)));
        return View{ready,either(full,complete),accept,finish,
            either(capture,both(full,inv(take))),either(accept,both(pending,inv(finish)))};
    };
    MemoryTimingGroup group;group.endpoint=endpoint;
    const bool refreshes=endpoint->technology()==memory::Technology::DRAM;
    // code = registers (27) + 27 * (pending + 3 * full + 9 * sequencer)
    // After reset the sequencer is not ready until its first edge.
    std::vector<unsigned> queue{0,243*Working};std::set<unsigned> seen{0,243*Working};size_t transitions=0;
    std::map<std::array<unsigned,4>,size_t> shown;
    for(size_t index=0;index<queue.size();++index) {
        unsigned digits=queue[index];
        MemoryTimingCase mode;mode.sequenced=true;
        std::array<unsigned,4> signature{};
        for(size_t i=0;i<3;++i){mode.registers[cells[i]->getOutputPin("Q")]=LogicValue(digits%3);digits/=3;}
        const auto pending=LogicValue(digits%3),full=LogicValue(digits/3%3);const unsigned seq=digits/9;
        const std::array<LogicValue,3> takes{L,H,X};
        for(size_t i=0;i<3;++i)signature[i]=unsigned(mode.ready[i]=view(pending,full,seq,takes[i],L).ready);
        signature[3]=unsigned(mode.valid=view(pending,full,seq,L,L).valid);
        // States in which the endpoint shows the same are one timing case,
        // with every value the requester's registers take among them.
        const auto [found,fresh]=shown.emplace(signature,group.cases.size());
        if(fresh){group.cases.push_back(mode);group.cases.back().registers.clear();}
        for(const auto& [pin,value]:mode.registers)
            group.cases[found->second].register_values[pin]|=uint8_t(1u<<unsigned(value));
        for(unsigned controls=0;controls<64;++controls) {
            Evaluate e;
            e.values=mode.registers;
            e.values[request->getInputPin("RST")]=L;
            e.values[request->getInputPin("ISSUE")]=LogicValue(controls%4);
            e.values[request->getInputPin("TAKE")]=LogicValue(controls/4%4);
            e.values[mismatch]=LogicValue(controls/16);
            const auto issue=clean(e.get(request->getOutputPin("BUS_REQ_VALID")));
            const auto take=clean(e.get(request->getOutputPin("BUS_RSP_READY")));
            const auto now=view(pending,full,seq,take,issue);
            e.values[request->getInputPin("BUS_REQ_READY")]=now.ready;
            e.values[request->getInputPin("BUS_RSP_VALID")]=now.valid;
            unsigned next=0,weight=1;
            for(size_t i=0;i<3;++i) {
                if(typeid(*cells[i])==typeid(FlipFlopCellBehavioral) &&
                    e.get(cells[i]->getInputPin("WE"))!=H)return std::nullopt;
                auto d=e.get(cells[i]->getInputPin("D"));if(d==LogicValue::HIGH_Z)d=X;
                next+=unsigned(d)*weight;weight*=3;
            }
            next+=27*(unsigned(now.pending)+3*unsigned(now.full));
            // The sequencer starts and finishes only on a clean 1.
            std::vector<unsigned> after;
            const bool start=now.accept==H;
            if(seq==Done&&now.finish!=H)after={Done};
            else if(seq!=Working) {
                after={start?Working:Idle};
                if(refreshes&&!start)after.push_back(Working);
            } else {
                after={Working};
                if(pending!=L)after.push_back(Done);
                if(pending!=H)after.push_back(Idle);
            }
            for(const auto s:after) {
                const unsigned code=next+243*s;
                if(seen.insert(code).second)queue.push_back(code);
                ++transitions;
            }
        }
    }
    group.evidence=endpoint->getID()+" + "+request->getID()+": exhaustive actual control-netlist reachability with the timed memory's clocked handshake; "
        +std::to_string(queue.size())+" states, "+std::to_string(group.cases.size())+" timing cases, "
        +std::to_string(transitions)+" four-state transitions";
    return group;
}
}
std::optional<MemoryTimingGroup> memoryTimingStates(
    const std::shared_ptr<BehavioralMemoryEndpoint>& endpoint,const ClockSpecification& clock) {
    try {
        // A timed memory is taken only with a delay table proven on its
        // gates that says its interface keeps to the clock.
        const bool sequenced=endpoint->technology()!=memory::Technology::FF;
        if(sequenced) {
            const auto* table=endpoint->getDelays();
            if(!table||!table->clocked_interface||clock.binary_startup)return std::nullopt;
        } else if(typeid(*endpoint)!=typeid(BehavioralMemoryEndpoint)
            &&typeid(*endpoint)!=typeid(CompactMemoryEndpoint<8,memory::FFModel>)
            &&typeid(*endpoint)!=typeid(CompactMemoryEndpoint<16,memory::FFModel>)
            &&typeid(*endpoint)!=typeid(CompactMemoryEndpoint<32,memory::FFModel>))return std::nullopt;
        MemoryTimingGroup group;group.endpoint=endpoint;
        const auto ready_source=source(endpoint->getInputPin("RSP_READY"));
        bool always_ready=false;
        if(ready_source->getType()==PinType::OUTPUT && typeid(*ready_source->getOwner())==typeid(ConstantValue<1>))
            always_ready=(static_cast<const ConstantValue<1>&>(*ready_source->getOwner()).getConstantValue()&1)!=0;
        for(const auto& input:clock.inputs)if(!input.edge && input.fixed_value==1)
            if(input.pin==ready_source->getID() || input.pin==ready_source->getOwner()->getID()+"."+ready_source->getName())always_ready=true;
        if(always_ready&&sequenced)return std::nullopt;
        if(always_ready) {
            // From reset full=0; with TAKE=1, full'=accept && full. Induction
            // holds even for nonbinary request controls (only HIGH accepts).
            group.cases.push_back({false,{}});
            group.evidence=endpoint->getID()+": RSP_READY fixed HIGH; FULL remains empty after reset (four-state induction)";
            return group;
        }
        auto owner=source(endpoint->getInputPin("REQ_VALID"))->getOwner();
        std::set<std::shared_ptr<Component>> parents;
        while(owner && typeid(*owner)!=typeid(RV32IMemoryRequest)) {
            if(!parents.insert(owner).second)return std::nullopt;
            owner=owner->getParent();
        }
        auto request=std::dynamic_pointer_cast<IOComponent>(owner);
        if(!request)return std::nullopt;
        if(source(request->getInputPin("BUS_REQ_READY"))!=endpoint->getOutputPin("REQ_READY")
            ||source(request->getInputPin("BUS_RSP_VALID"))!=endpoint->getOutputPin("RSP_VALID")
            ||ready_source!=source(request->getOutputPin("BUS_RSP_READY"))
            ||source(endpoint->getInputPin("CLK"))!=source(request->getInputPin("CLK"))
            ||source(endpoint->getInputPin("RST"))!=source(request->getInputPin("RST")))return std::nullopt;
        const std::array cells{child(request,"BUSY"),child(request,"SENT"),child(request,"ABANDONED")};
        const auto mismatch=child(request,"TAG_MATCH")->getOutputPin("CHANGED");
        if(clock.binary_startup)
            group.binary_controls={request->getInputPin("ISSUE"),request->getInputPin("TAKE"),mismatch};
        for(const auto& cell:cells) {
            if((typeid(*cell)!=typeid(FlipFlopCellBehavioral) &&
                typeid(*cell)!=typeid(ClockedStateBitBehavioral))
                ||source(cell->getInputPin("CLK"))!=source(request->getInputPin("CLK"))
                ||source(cell->getInputPin("RST"))!=source(request->getInputPin("RST")))return std::nullopt;
        }
        if(sequenced)return sequencedStates(endpoint,request,cells,mismatch);
        // Three ternary registers (Z captures as X) and one binary endpoint
        // FULL bit: at most 54 states. Reset is the seed; operation holds RST=0.
        std::vector<unsigned> queue{0};std::set<unsigned> seen{0};size_t transitions=0;
        for(size_t index=0;index<queue.size();++index) {
            unsigned code=queue[index],digits=code;
            std::array<LogicValue,3> q;
            MemoryTimingCase mode;
            for(size_t i=0;i<3;++i){q[i]=LogicValue(digits%3);digits/=3;mode.registers[cells[i]->getOutputPin("Q")]=q[i];}
            mode.full=digits!=0;group.cases.push_back(mode);
            const unsigned radix=clock.binary_startup?2:4;
            for(unsigned controls=0;controls<radix*radix*radix;++controls) {
                Evaluate e;
                e.values=mode.registers;
                e.values[request->getInputPin("RST")]=L;
                e.values[request->getInputPin("ISSUE")]=LogicValue(controls%radix);
                e.values[request->getInputPin("TAKE")]=LogicValue((controls/radix)%radix);
                e.values[mismatch]=LogicValue(controls/(radix*radix));
                const bool valid=e.get(request->getOutputPin("BUS_REQ_VALID"))==H;
                const bool take=e.get(request->getOutputPin("BUS_RSP_READY"))==H;
                const bool ready=!mode.full||take,accept=valid&&ready;
                e.values[request->getInputPin("BUS_REQ_READY")]=ready?H:L;
                e.values[request->getInputPin("BUS_RSP_VALID")]=mode.full||valid?H:L;
                unsigned next=0,weight=1;
                for(size_t i=0;i<3;++i) {
                    if(typeid(*cells[i])==typeid(FlipFlopCellBehavioral) &&
                        e.get(cells[i]->getInputPin("WE"))!=H)return std::nullopt;
                    auto d=e.get(cells[i]->getInputPin("D"));if(d==LogicValue::HIGH_Z)d=X;
                    next+=unsigned(d)*weight;weight*=3;
                }
                const bool capture=accept&&(mode.full||!take);
                const bool full=capture||(mode.full&&!take);
                next+=full?27:0;
                if(seen.insert(next).second)queue.push_back(next);
                ++transitions;
            }
        }
        group.evidence=endpoint->getID()+" + "+request->getID()+": exhaustive actual control-netlist reachability; "
            +std::to_string(queue.size())+" states, "+std::to_string(transitions)
            +(clock.binary_startup?" binary transitions (conditional on preservation proof)":" four-state transitions");
        return group;
    } catch(const std::exception&) {return std::nullopt;}
}
}
