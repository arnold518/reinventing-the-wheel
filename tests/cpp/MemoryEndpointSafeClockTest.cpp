#include "analysis/SafeClockTiming.hpp"
#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "components/selection/BuiltinComponentCatalog.hpp"
#include "components/selection/StandardProfiles.hpp"
#include "modules/memory/BehavioralMemoryEndpoint.hpp"
#include "simulator/Event.hpp"
#include "simulator/Simulator.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

using namespace circuit::analysis;
using namespace circuit::memory;
using namespace circuit::memory_device;
namespace {
void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
const std::vector<std::string> inputs{
    "REQ_VALID", "REQ_OP", "REQ_ADDR", "REQ_WRITE_DATA", "REQ_WRITE_MASK", "RSP_READY"};
const std::vector<std::string> outputs{"REQ_READY", "RSP_VALID", "RSP_READ_DATA", "RSP_STATUS"};
const std::set<std::pair<std::string, std::string>> dependencies{
    {"RSP_READY", "REQ_READY"}, {"REQ_VALID", "RSP_VALID"},
    {"REQ_VALID", "RSP_READ_DATA"}, {"REQ_OP", "RSP_READ_DATA"},
    {"REQ_ADDR", "RSP_READ_DATA"}, {"REQ_OP", "RSP_STATUS"},
    {"REQ_ADDR", "RSP_STATUS"}, {"REQ_WRITE_MASK", "RSP_STATUS"}};
ClockSpecification specification(const IOComponent& c) {
    ClockSpecification s; s.clock_pin = c.getID() + ".CLK";
    s.inputs.push_back({c.getID()+".RST",std::nullopt,{},0});
    for (const auto& i : inputs) s.inputs.push_back({c.getID()+"."+i,std::nullopt,{},0});
    for (const auto& o : outputs) s.outputs.push_back({c.getID()+"."+o,ClockEdge::Rising,0,0});
    return s;
}
void safe(const ClockTimingReport& r, size_t period) {
    for (const auto& d : r.diagnostics) std::cerr << d << '\n';
    check(r.status == ClockTimingStatus::SafeUnderContracts && r.safe_period_ticks == period,
          "Expected safe endpoint period " + std::to_string(period));
}
void unknown(const ClockTimingReport& r) {
    check(r.status == ClockTimingStatus::Unknown && !r.safe_period_ticks, "Unsupported endpoint must fail closed");
}
class ModifiedEndpoint : public BehavioralMemoryEndpoint {
public:
    using BehavioralMemoryEndpoint::BehavioralMemoryEndpoint;
    void evaluate(size_t, Simulator&) override {}
};
void staticCoverage() {
    for (size_t width : {8,16,32}) {
        const auto& catalog = circuit::builtinComponentCatalog();
        // Both catalog families use the SAME fused implementation and contract.
        for (auto family : {"memory.ff.endpoint.", "memory.ff.scalable-endpoint."}) {
            auto c = std::dynamic_pointer_cast<IOComponent>(catalog.createRoot(
                {family+std::to_string(width),"MEM",{}, {}, {}, {}}, circuit::strictAllBehavioral()).root);
            auto base = specification(*c);
            safe(computeSafeClockPeriod(c,base),2);
            for (size_t i=0;i<inputs.size();++i) {
                auto s=base; s.inputs[i+1].edge=ClockEdge::Rising; s.inputs[i+1].delay={1,9};
                safe(computeSafeClockPeriod(c,s), inputs[i]=="REQ_WRITE_DATA" ? 10 : 12);
                for (size_t j=0;j<outputs.size();++j) {
                    auto probe=s; probe.outputs[j].setup=20;
                    safe(computeSafeClockPeriod(c,probe),dependencies.contains({inputs[i],outputs[j]})?32:22);
                    probe.inputs[i+1].edge=ClockEdge::Falling;
                    safe(computeSafeClockPeriod(c,probe),dependencies.contains({inputs[i],outputs[j]})?62:22);
                }
                s.inputs[i+1].delay={0,9};
                const auto hold=computeSafeClockPeriod(c,s);
                check(hold.status==ClockTimingStatus::TimingViolation && !hold.safe_period_ticks,
                      "All request/control inputs must be rising-edge captures");
            }
            base.inputs[0].fixed_value=1; unknown(computeSafeClockPeriod(c,base));
        }
        for (auto tech : {Technology::SRAM,Technology::DRAM,Technology::ROM,Technology::NORFlash}) {
            auto c=Component::create<BehavioralMemoryEndpoint>("MEM",width,tech);
            unknown(computeSafeClockPeriod(c,specification(*c)));
        }
    }
    auto checkCompact=[]<size_t W>() {
        auto c=Component::create<CompactMemoryEndpoint<W,FFModel>>("MEM");
        safe(computeSafeClockPeriod(c,specification(*c)),2);
    };
    checkCompact.template operator()<8>(); checkCompact.template operator()<16>(); checkCompact.template operator()<32>();
    auto modified=Component::create<ModifiedEndpoint>("MEM",8,Technology::FF);
    unknown(computeSafeClockPeriod(modified,specification(*modified)));
}

// Independent tiny FF endpoint oracle: no production validator, controller or
// native device engine is called. It models the response queue and four words.
struct Oracle {
    size_t width;
    std::vector<Bits> words;
    bool full=false;
    LogicValue previous=X;
    Bits saved;
    unsigned saved_status=0;
    explicit Oracle(size_t w):width(w),words(4,Bits(w,X)),saved(w,L) {}
    unsigned status(const std::map<std::string,Bits>& p) const {
        if(!known(p.at("REQ_OP"))||!known(p.at("REQ_ADDR")))return 6;
        const auto op=number(p.at("REQ_OP")),addr=number(p.at("REQ_ADDR"));
        if(op>3)return 4;
        if((op==1||op==2)&&!known(p.at("REQ_WRITE_MASK")))return 6;
        if(addr%(width/8))return 1;
        if(addr/(width/8)>=words.size())return 2;
        return op>1?4:0;
    }
    std::map<std::string,Bits> step(const std::map<std::string,Bits>& p) {
        auto high=[&](const std::string& n){return p.at(n)[0]==H;};
        const bool edge=previous==L&&high("CLK"); previous=p.at("CLK")[0];
        const bool reset=p.at("RST")[0]!=L, take=high("RSP_READY"), valid=high("REQ_VALID");
        const auto st=status(p),op=number(p.at("REQ_OP")),addr=number(p.at("REQ_ADDR"));
        const bool accepted=!reset&&(!full||take)&&valid;
        Bits read(width,L);
        if(st==0&&op==0)read=words[addr/(width/8)];
        if(reset){full=false;saved.assign(width,L);saved_status=0;}
        else if(edge) {
            if(accepted&&(full||!take)){saved=read;saved_status=st;full=true;}
            else if(take)full=false;
            if(accepted&&st==0&&op==1)
                for(size_t bit=0;bit<width;++bit)if(p.at("REQ_WRITE_MASK")[bit/8]==H)
                    words[addr/(width/8)][bit]=stored(p.at("REQ_WRITE_DATA")[bit]);
        }
        return {{"REQ_READY",{!reset&&(!full||take)?H:L}},
                {"RSP_VALID",{!reset&&(full||valid)?H:L}},
                {"RSP_READ_DATA",full?saved:(!reset&&valid?read:Bits(width,L))},
                {"RSP_STATUS",bits(3,full?saved_status:st)}};
    }
};
struct Fixture {
    std::shared_ptr<BehavioralMemoryEndpoint> c;
    Simulator sim;
    std::map<std::string,std::shared_ptr<WireBase>> wires;
    std::map<std::string,Bits> pins;
    Oracle oracle;
    size_t time=0,steps=0;
    std::set<std::pair<std::string,std::string>> witnessed;
    explicit Fixture(size_t width):c(Component::create<BehavioralMemoryEndpoint>("MEM",width,Technology::FF)),oracle(width) {
        c->setHistoryRecordingEnabled(false); sim.setHistoryRecordingEnabled(false);
        ComponentBuilder b(c);
        for(const auto& [n,p]:c->getAllInputPins()) {
            wires[n]=b.addNewWireDynamic("drive_"+n,p->getWidth(),nullptr,{p});
            pins[n]=Bits(p->getWidth(),L);
        }
        for(const auto& [n,p]:c->getAllOutputPins())
            wires[n]=b.addNewWireDynamic("observe_"+n,p->getWidth(),p,{});
        step({{"RST",{H}}});step({{"RST",{L}}});
    }
    void schedule(const std::string& n,const Bits& v) {
        const auto& w=wires.at(n);
        auto add=[&]<size_t W>() {sim.scheduleEvent(std::make_shared<WireUpdateEvent<W>>(
            time,std::dynamic_pointer_cast<Wire<W>>(w),v));};
        switch(v.size()) {
        case 1:add.template operator()<1>();break;case 2:add.template operator()<2>();break;
        case 3:add.template operator()<3>();break;case 4:add.template operator()<4>();break;
        case 8:add.template operator()<8>();break;case 16:add.template operator()<16>();break;
        case 32:add.template operator()<32>();break;default:throw std::runtime_error("Bad test width");
        }
    }
    void step(const std::map<std::string,Bits>& changes) {
        time+=4;
        std::map<std::string,Bits> before;
        for(const auto& o:outputs)before[o]=wires.at(o)->getValueVector();
        std::set<std::string> changed;
        // Drive all initial inputs, then only the requested updates.
        auto next=pins;
        for(const auto& [n,v]:changes)next[n]=v;
        for(const auto& [n,v]:next)if(!steps||v!=pins.at(n)){changed.insert(n);schedule(n,v);}
        pins=std::move(next);
        const auto expected=oracle.step(pins);
        sim.runAndRecord(time);
        for(const auto& o:outputs)check(wires.at(o)->getValueVector()==before.at(o),"Output changed before its one-tick delay");
        sim.runAndRecord(time+1);
        for(const auto& o:outputs) {
            const auto actual=wires.at(o)->getValueVector();
            check(actual==expected.at(o),"FF endpoint oracle mismatch: "+o+" at "+std::to_string(time));
            if(actual!=before.at(o)&&steps&& !changed.contains("RST")&&!changed.contains("CLK")) {
                bool covered=false;
                for(const auto& i:changed)if(dependencies.contains({i,o})) {
                    covered=true;if(changed.size()==1)witnessed.insert({i,o});
                }
                check(covered,"Observed combinational transition missing from contract: "+o);
            }
        }
        sim.runAndRecord(time+3);
        check(!sim.hasPendingEvents(),"FF endpoint must not have autonomous delayed work");
        ++steps;
    }
};
void transitions() {
    size_t cases=0,steps=0;
    std::set<std::pair<std::string,std::string>> witnessed;
    for(size_t width:{8,16,32}) {
        Fixture f(width);
        auto request=[&](unsigned op,uint32_t addr,uint32_t data,const Bits& mask) {
            f.step({{"REQ_OP",bits(3,op)},{"REQ_ADDR",bits(32,addr)},
                    {"REQ_WRITE_DATA",bits(width,data)},{"REQ_WRITE_MASK",mask}});
        };
        // Single-pin changes witness every declared combinational arc, including
        // address errors and a nonbinary write mask. Clock stays LOW throughout.
        f.step({{"REQ_VALID",{H}}});
        f.step({{"REQ_OP",bits(3,1)}}); // Unknown memory read -> zero write response.
        f.step({{"REQ_WRITE_MASK",Bits(width/8,X)}});
        f.step({{"REQ_WRITE_MASK",Bits(width/8,L)}});
        f.step({{"REQ_OP",bits(3,0)}});
        f.step({{"REQ_ADDR",bits(32,4*(width/8))}});
        f.step({{"REQ_ADDR",bits(32,0)}});
        f.step({{"CLK",{H}}}); // Save a response under backpressure.
        f.step({{"CLK",{L}}});f.step({{"RSP_READY",{H}}});f.step({{"RSP_READY",{L}}});
        for(bool held:{false,true})for(unsigned valid:{0,1})for(unsigned ready:{0,1})
        for(unsigned op=0;op<8;++op)for(uint32_t addr:{0u,uint32_t(3*(width/8)),1u,uint32_t(4*(width/8))})
        for(unsigned mask_kind=0;mask_kind<3;++mask_kind) {
            ++cases;
            f.step({{"CLK",{L}},{"RST",{H}}});
            f.step({{"RST",{L}},{"REQ_VALID",{H}},{"RSP_READY",{held?L:H}}});
            request(0,0,0,Bits(width/8,L));
            if(held){f.step({{"CLK",{H}}});f.step({{"CLK",{L}}});}
            const Bits mask(width/8,mask_kind==0?L:mask_kind==1?H:X);
            request(op,addr,0xa55a3cc3,mask);
            f.step({{"REQ_VALID",{valid?H:L}},{"RSP_READY",{ready?H:L}}});
            f.step({{"CLK",{H}}}); // Bypass / hold / drain / replace / write.
            f.step({{"CLK",{L}}});
            f.step({{"REQ_ADDR",bits(32,0)},{"REQ_OP",bits(3,0)}});
        }
        // Nonbinary payload storage and invalid request fields are also timed.
        f.step({{"CLK",{L}},{"RST",{H}}});
        f.step({{"RST",{L}},{"REQ_VALID",{H}},{"RSP_READY",{H}}});
        request(1,0,0,Bits(width/8,H));
        f.step({{"REQ_WRITE_DATA",Bits(width,Z)}});f.step({{"CLK",{H}}});f.step({{"CLK",{L}}});
        f.step({{"REQ_OP",bits(3,0)}});
        f.step({{"REQ_ADDR",Bits(32,X)}});f.step({{"REQ_ADDR",bits(32,0)}});
        f.step({{"REQ_OP",Bits(3,Z)}});
        witnessed.insert(f.witnessed.begin(),f.witnessed.end());steps+=f.steps;
    }
    check(witnessed==dependencies,"Every endpoint dependency must have a dynamic witness");
    std::cout<<cases<<" endpoint state/request cases, "<<steps<<" timed transitions; all 8 arcs witnessed\n";
}
}
int main() {
    try {staticCoverage();transitions();}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
