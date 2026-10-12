#include "modules/rv32i/RV32IPipelineObservation.hpp"
#include "modules/composite/ALU32Behavioral.hpp"
#include "RV32IBaselinePipelineInternal.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.tpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/IncrementBy4.hpp"
#include "modules/storage/RegisterBehavioral.hpp"
#include "basic/LogicValueHelpers.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include "modules/rv32i/RegisterFile32x32Behavioral.hpp"
#include <limits>
#include <set>

namespace rv32i::baseline {
namespace {
using namespace pipeline_encoding;
uint32_t get(const Values& v,const std::string& n) {auto i=v.find(n);return i==v.end()?0:i->second;}
void carry(const Values& in,Values& out,const std::vector<std::string>& fields) {
    out["OUT_VALID"]=get(in,"IN_VALID");for(auto& f:fields) out["OUT_"+f]=get(in,"IN_"+f);
}
bool terminal(const Values& in) {
    const auto c=unpackControl(get(in,"IN_CONTROL"));
    return get(in,"IN_VALID") && ((get(in,"IN_FETCH_STATUS")&3)||!c.legal||c.halt||c.trap||(get(in,"IN_EXECUTION_STATUS")&1));
}
uint32_t wbValue(const Values& in) {
    switch(bits(get(in,"IN_CONTROL"),9,3)) {
        case 1:return get(in,"IN_ALU_RESULT");case 2:return get(in,"IN_MEMORY_DATA");
        case 3:return get(in,"IN_PC_PLUS_4");default:return 0;
    }
}
unsigned trapCause(const Values& in) {
    if(!get(in,"IN_VALID")) return 0;
    const auto c=unpackControl(get(in,"IN_CONTROL"));
    const auto f=get(in,"IN_FETCH_STATUS"),m=get(in,"IN_MEMORY_STATUS"),e=get(in,"IN_EXECUTION_STATUS");
    if(f&1) return 3;
    if(f&2) return 4;
    if(!c.legal) return 1;
    if(c.trap) return c.trap_cause==RV32ITrapCause::EnvironmentCall?2:1;
    if(c.mem_read&&(m&1)) return 5;
    if(c.mem_read&&(m&2)) return 6;
    if(c.mem_write&&(m&1)) return 7;
    if(c.mem_write&&(m&2)) return 8;
    return e&1?3:0;
}
std::vector<LogicValue> logicBits(size_t n,uint32_t value) {
    std::vector<LogicValue> r(n);for(size_t i=0;i<n;++i) r[i]=value&(uint32_t{1}<<i)?LogicValue::HIGH:LogicValue::LOW;return r;
}
class BinaryBlock : public BasicComponent {
public:
    BinaryBlock(std::string n,const circuit::PinInitializer& pins):BasicComponent(std::move(n),1,pins) {}
protected:
    Values inputs() const {
        Values v;for(auto& [n,p]:getAllInputPins()) {
            uint32_t value=0;const auto bits=p->getValueAsVector();
            for(size_t i=0;i<bits.size();++i) if(bits[i]==LogicValue::HIGH) value|=uint32_t{1}<<i;
            v[n]=value;
        }return v;
    }
    bool rising() {auto c=getInputPin("CLK")->getValue();bool e=previous_==LogicValue::LOW&&c==LogicValue::HIGH;previous_=c;return e;}
    void emit(const Values& values,size_t t,Simulator& s,const std::set<std::string>& skip={}) {
        for(auto& [n,p]:getAllOutputPins()) {
            if(skip.contains(n)) continue;
            auto v=get(values,n);
            switch(p->getWidth()) {
                case 1:_updateOutputWire(s,n,v?LogicValue::HIGH:LogicValue::LOW,t);break;
                case 2:_updateOutputWire<2>(s,n,v,t);break;
                case 3:_updateOutputWire<3>(s,n,v,t);break;
                case 4:_updateOutputWire<4>(s,n,v,t);break;
                case 32:_updateOutputWire<32>(s,n,v,t);break;
                default:throw std::logic_error("Unsupported baseline output width");
            }
        }
    }
private:
    LogicValue previous_=LogicValue::UNKNOWN;
};
}

Values decodePacket(const Values& in,const std::array<uint32_t,32>& regs) {
    Values o;carry(in,o,fetchFields);
    const auto d=RV32IDecoder::decode(get(in,"IN_INSTRUCTION"));const auto c=RV32IControl::fromDecoded(d);
    o["OUT_CONTROL"]=packControl(c);o["OUT_REGISTER_ADDRESSES"]=packAddresses(d.rs1,d.rs2,d.rd);
    o["OUT_IMMEDIATE"]=uint32_t(d.immediate);o["OUT_RS1_VALUE"]=d.rs1?regs[d.rs1]:0;o["OUT_RS2_VALUE"]=d.rs2?regs[d.rs2]:0;
    o["TERMINAL"]=get(in,"IN_VALID")&&((get(in,"IN_FETCH_STATUS")&3)||!c.legal||c.halt||c.trap);
    o["CONTROL_FLOW"]=get(in,"IN_VALID")&&(c.branch!=RV32IBranchType::None||c.jump!=RV32IJumpType::None);return o;
}
Values executePacket(const Values& in) {
    Values o;carry(in,o,{"PC","INSTRUCTION","CONTROL","REGISTER_ADDRESSES","FETCH_STATUS"});
    const auto word=get(in,"IN_CONTROL"),as=bits(word,27,3),bs=bits(word,29,3),op=bits(word,22,31);
    const auto a=as==0?get(in,"IN_RS1_VALUE"):as==1?get(in,"IN_PC"):0;
    const auto b=bs==0?get(in,"IN_RS2_VALUE"):bs==1?get(in,"IN_IMMEDIATE"):0;
    const auto result=alu32Result(op,a,b),jump=bits(word,16,3),branch=bits(word,13,7);
    const bool cmp=op==ALU32Op::SUB||op==ALU32Op::SLT||op==ALU32Op::SLTU;
    const bool eq=cmp&&a==b,lt=cmp&&int32_t(a)<int32_t(b),ltu=cmp&&a<b;
    const bool conditions[]{false,eq,!eq,lt,!lt,ltu,!ltu,false};
    const bool taken=jump||conditions[branch];
    const uint32_t target=jump==0?get(in,"IN_PC")+get(in,"IN_IMMEDIATE"):jump==1?result:jump==2?result&~uint32_t{1}:0;
    o["OUT_ALU_RESULT"]=result;o["OUT_STORE_DATA"]=get(in,"IN_RS2_VALUE");o["OUT_PC_PLUS_4"]=get(in,"IN_PC")+4;
    o["OUT_NEXT_PC"]=taken?target:o["OUT_PC_PLUS_4"];o["OUT_EXECUTION_STATUS"]=taken&&bool(target&3);
    o["TERMINAL"]=terminal(in)||(get(in,"IN_VALID")&&o["OUT_EXECUTION_STATUS"]);
    o["CONTROL_FLOW"]=get(in,"IN_VALID")&&(jump||branch);return o;
}
Values memoryPacket(const Values& in) {
    Values o;carry(in,o,executeFields);
    const auto word=get(in,"IN_CONTROL"),size=bits(word,11,3),addr=get(in,"IN_ALU_RESULT"),offset=addr&3;
    const bool memread=bits(word,2,1),memwrite=bits(word,3,1),sign=bits(word,4,1);
    const bool early=terminal(in),eligible=get(in,"IN_VALID")&&!early&&(memread||memwrite);
    const bool bad=size==3||(size==1&&(addr&1))||(size==2&&(addr&3));
    const bool issue=eligible&&get(in,"MEMORY_ENABLE"),fault=get(in,"DMEM_FAULT");
    auto value=get(in,"DMEM_READ_DATA")>>(offset*8);
    if(size==0) {value&=255;if(sign&&(value&128)) value|=0xffffff00U;}
    else if(size==1) {value&=65535;if(sign&&(value&32768)) value|=0xffff0000U;}
    else if(size==3) value=0;
    o["OUT_MEMORY_DATA"]=value;o["OUT_MEMORY_STATUS"]=eligible?(uint32_t(bad)|(uint32_t(fault)<<1)):0;
    o["TERMINAL"]=early||(eligible&&(bad||fault));
    o["DMEM_WORD_ADDR"]=addr>>2;o["DMEM_READ_EN"]=issue;o["DMEM_WE"]=issue&&memwrite&&!bad;
    o["DMEM_WRITE_DATA"]=get(in,"IN_STORE_DATA")<<(offset*8);
    o["DMEM_WRITE_MASK"]=size==0?1U<<offset:size==1?3U<<(addr&2):size==2?15:0;return o;
}
Values dependency(const Values& in) {
    Values o;const auto d=get(in,"D_CONTROL"),a=get(in,"D_REGISTER_ADDRESSES");
    for(const std::string stage:{"E","M","W"}) {
        const auto dest=rd(get(in,stage+"_REGISTER_ADDRESSES"));
        const bool h=get(in,"D_VALID")&&get(in,stage+"_VALID")&&bits(get(in,stage+"_CONTROL"),1,1)&&dest&&
            ((bits(d,7,1)&&rs1(a)==dest)||(bits(d,8,1)&&rs2(a)==dest));
        o[stage=="E"?"WAIT_EX":stage=="M"?"WAIT_MEM":"WAIT_WB"]=h;o["DATA_HAZARD"]|=h;
    }return o;
}
Values control(const Values& in) {
    auto p=[&](const char* n){return get(in,n)!=0;};Values o;
    const bool active=p("ENABLE")&&!p("RST")&&!p("HALTED")&&!p("TRAPPED");
    const bool late=p("M_TERMINAL")||p("W_TERMINAL"),older=p("E_TERMINAL")||late;
    const bool term=older||(p("D_TERMINAL")&&!p("E_CONTROL_FLOW"));
    const bool stall=p("DATA_HAZARD")&&!p("D_TERMINAL")&&!older;
    const bool branch=p("D_CONTROL_FLOW")||p("E_CONTROL_FLOW");
    const bool fetch=active&&!term&&!stall&&!branch,redirect=active&&p("E_CONTROL_FLOW")&&!older;
    o["ACTIVE"]=o["COMMIT_ENABLE"]=active;o["FETCH_VALID"]=fetch;o["PC_WRITE"]=fetch||redirect;o["PC_REDIRECT"]=redirect;
    o["IF_ID_WRITE"]=active&&(!stall||p("E_CONTROL_FLOW"));o["IF_ID_FLUSH"]=term||branch;
    o["ID_EX_WRITE"]=o["EX_MEM_WRITE"]=o["MEM_WB_WRITE"]=active;
    o["ID_EX_FLUSH"]=stall||older||p("E_CONTROL_FLOW");o["EX_MEM_FLUSH"]=late;o["MEM_WB_FLUSH"]=p("W_TERMINAL");
    o["MEMORY_ENABLE"]=active&&!p("W_TERMINAL");o["DATA_STALL"]=active&&stall;
    o["BRANCH_WAIT"]=active&&branch;o["PIPELINE_FLUSH"]=active&&(term||branch);return o;
}

namespace {
class BehavioralStage : public BinaryBlock {
public:
    BehavioralStage(std::string n,Block b):BinaryBlock(std::move(n),family(b).pinInitializer()),block_(b) {record(0);}
    const char* getTypeName() const override {return family(block_).typeName().data();}
    std::vector<std::vector<LogicValue>> registerStateAtTime(size_t time) const {
        auto i=history_.upper_bound(time);return i==history_.begin()?i->second:std::prev(i)->second;
    }
    void evaluate(size_t t,Simulator& s) override {
        if(block_==Block::FetchStage) {fetch(t,s);return;}
        auto in=inputs();Values out;
        if(block_==Block::DecodeStage) {
            if(!s.isHistoryRecordingEnabled() && history_.size()>1) {
                auto latest=*history_.rbegin();history_.clear();history_.insert(std::move(latest));
            }
            const auto before=words_;
            rv32i::register_file::step(words_,previous_clock_,getInputValue("RST"),getInputValue("CLK"),
                getInputValue("REGISTER_WRITE"),getInputPin<5>("WB_RD")->getValueAsVector(),
                getInputPin<32>("WB_VALUE")->getValueAsVector());
            if(words_!=before) record(t+delay);
        }
        switch(block_) {
            case Block::FetchStage:break;
            case Block::DecodeStage: {
                // A stored bit the gates do not know stays unknown on the way out.
                std::array<uint32_t,32> known{};
                out=decodePacket(in,known);
                const auto a=get(out,"OUT_REGISTER_ADDRESSES");
                emit(out,t,s,{"OUT_RS1_VALUE","OUT_RS2_VALUE"});
                _updateOutputWire<32>(s,"OUT_RS1_VALUE",words_[rs1(a)],t);
                _updateOutputWire<32>(s,"OUT_RS2_VALUE",words_[rs2(a)],t);
                return;
            }
            case Block::ExecuteStage:out=executePacket(in);break;
            case Block::MemoryStage:out=memoryPacket(in);break;
            case Block::DependencyUnit:out=dependency(in);break;
            case Block::Control:out=control(in);break;
        }
        emit(out,t,s);
    }
private:
    Block block_;std::vector<LogicValue> pc_=std::vector<LogicValue>(32,LogicValue::UNKNOWN);
    LogicValue fetch_clock_=LogicValue::UNKNOWN,previous_clock_=LogicValue::UNKNOWN;
    std::array<std::vector<LogicValue>,32> words_=[] {
        std::array<std::vector<LogicValue>,32> w;w.fill(std::vector<LogicValue>(32,LogicValue::UNKNOWN));
        w[0].assign(32,LogicValue::LOW);return w;}();
    // A register, an incrementer, a multiplexer and wires, each by its own
    // four-state rule.
    void fetch(size_t t,Simulator& s) {
        using namespace circuit::logic_values;
        const auto redirect=getInputPin<32>("REDIRECT_PC")->getValueAsVector();
        const auto select=getInputValue("PC_REDIRECT");
        auto next=incrementBy4(pc_);
        for(size_t i=0;i<32;++i) next[i]=mux2(next[i],sanitizeBit(redirect[i]),select);
        registerStep(pc_,fetch_clock_,getInputValue("RST"),getInputValue("CLK"),getInputValue("PC_WRITE"),next);
        std::vector<LogicValue> word_address(32,LogicValue::LOW),status(32,LogicValue::LOW);
        for(size_t i=2;i<32;++i) word_address[i-2]=pc_[i];
        status[0]=orGate(pc_[0],pc_[1]);status[1]=getInputValue("IMEM_FAULT");
        _updateOutputWire<32>(s,"FETCH_PC",pc_,t);_updateOutputWire<32>(s,"OUT_PC",pc_,t);
        _updateOutputWire<32>(s,"OUT_INSTRUCTION",getInputPin<32>("IMEM_READ_DATA")->getValueAsVector(),t);
        _updateOutputWire(s,"OUT_VALID",getInputValue("FETCH_VALID"),t);
        _updateOutputWire(s,"IMEM_READ_EN",getInputValue("FETCH_VALID"),t);
        _updateOutputWire<32>(s,"OUT_FETCH_STATUS",status,t);_updateOutputWire<32>(s,"IMEM_WORD_ADDR",word_address,t);
    }
    std::map<size_t,std::vector<std::vector<LogicValue>>> history_;
    void record(size_t t) {std::vector<std::vector<LogicValue>> v(words_.begin(),words_.end());if(history_.empty()||history_.rbegin()->second!=v) history_[t]=std::move(v);}
};

// Only Decode owns architectural registers. Do not advertise a register-file
// inspector on combinational stages or on Fetch merely to share implementation.
class BehavioralDecode : public BehavioralStage, public RegisterStateView {
public:
    explicit BehavioralDecode(std::string n):BehavioralStage(std::move(n),Block::DecodeStage) {}
    std::vector<std::vector<LogicValue>> getRegisterStateAtTime(size_t t) const override {
        return registerStateAtTime(t);
    }
};

class BehavioralCore : public BinaryBlock,public RV32IPipelineStateView,public RV32IRetirementView {
public:
    explicit BehavioralCore(std::string n):BinaryBlock(std::move(n),circuit::families::RV32IBaselinePipelineCore.pinInitializer()) {}
    const char* getTypeName() const override {return "RV32IBaselinePipelineCore";}
    rv32i::RV32IArchitecturalState snapshotPipelineState(uint32_t observed_pc) const override {
        rv32i::RV32IArchitecturalState s;s.pc=logicBits(32,observed_pc);for(size_t i=0;i<32;++i) s.x[i]=logicBits(32,regs_[i]);
        s.halted=halted_?LogicValue::HIGH:LogicValue::LOW;s.trapped=trapped_?LogicValue::HIGH:LogicValue::LOW;s.trap_cause=logicBits(4,cause_);return s;
    }
    RV32IRetirementObservation pendingRetirement() const override {
        return {get(w_,"IN_PC"), get(w_,"IN_INSTRUCTION"), rv32i::observeMemoryRetirement(
            unpackControl(get(w_,"IN_CONTROL")), get(w_,"IN_ALU_RESULT"),
            get(w_,"IN_STORE_DATA"), get(w_,"IN_MEMORY_DATA"), (get(w_,"IN_MEMORY_STATUS")&3U)!=0), get(w_,"IN_NEXT_PC"),
            bool(trapCause(w_) || bits(get(w_,"IN_CONTROL"),5,1))};
    }
    void evaluate(size_t t,Simulator& s) override {
        const auto in=inputs();const bool edge=rising();
        if(get(in,"RST")) {regs_.fill(0);fetch_pc_=cause_=0;halted_=trapped_=false;d_.clear();e_.clear();m_.clear();w_.clear();}
        else if(edge) advance(in);
        auto [d,e,m,c]=comb(in);Values out;
        for(auto& [p,v]:m) if(p.starts_with("DMEM_")) out[p]=v;
        out["FETCH_PC"]=fetch_pc_;out["HALTED"]=halted_;out["TRAPPED"]=trapped_;out["TRAP_CAUSE"]=cause_;
        out["IMEM_REQ_ADDR"]=fetch_pc_&~3u;out["IMEM_REQ_VALID"]=get(c,"FETCH_VALID");out["INSTRUCTION_ATTEMPT"]=get(c,"FETCH_VALID");
        out["IMEM_RSP_READY"]=out["DMEM_RSP_READY"]=1;
        out["DMEM_REQ_ADDR"]=get(m,"DMEM_WORD_ADDR")<<2;
        out["DMEM_REQ_OP"]=get(m,"DMEM_WE");out["DMEM_REQ_VALID"]=get(m,"DMEM_READ_EN")||get(m,"DMEM_WE");
        out["DMEM_REQ_WRITE_DATA"]=get(m,"DMEM_WRITE_DATA");out["DMEM_REQ_WRITE_MASK"]=get(m,"DMEM_WRITE_MASK");
        out["COMMIT_VALID"]=get(c,"ACTIVE")&&get(w_,"IN_VALID");out["DATA_STALL"]=out["PIPELINE_STALL"]=get(c,"DATA_STALL");
        out["BRANCH_WAIT"]=get(c,"BRANCH_WAIT");out["PIPELINE_FLUSH"]=get(c,"PIPELINE_FLUSH");
        for(auto [name,p]:std::vector<std::pair<std::string,const Values*>>{{"IF_ID",&d_},{"ID_EX",&e_},{"EX_MEM",&m_},{"MEM_WB",&w_}}) {
            out[name+"_VALID"]=get(*p,"IN_VALID");out[name+"_PC"]=get(*p,"IN_PC");
        }emit(out,t,s);
    }
private:
    std::array<uint32_t,32> regs_{};uint32_t fetch_pc_=0,cause_=0;bool halted_=false,trapped_=false;
    Values d_,e_,m_,w_;
    struct Combinational {Values d,e,m,c;};
    Combinational comb(const Values& in) const {
        auto d=decodePacket(d_,regs_),e=executePacket(e_);auto mi=m_;
        const bool wt=get(w_,"IN_VALID")&&(trapCause(w_)||bits(get(w_,"IN_CONTROL"),5,1));
        mi["DMEM_READ_DATA"]=get(in,"DMEM_RSP_READ_DATA");mi["DMEM_FAULT"]=get(in,"DMEM_RSP_STATUS")!=0;
        mi["MEMORY_ENABLE"]=get(in,"ENABLE")&&!get(in,"RST")&&!halted_&&!trapped_&&!wt;
        auto m=memoryPacket(mi);Values h;
        h["D_VALID"]=get(d_,"IN_VALID");h["D_CONTROL"]=get(d,"OUT_CONTROL");h["D_REGISTER_ADDRESSES"]=get(d,"OUT_REGISTER_ADDRESSES");
        for(auto [name,p]:std::vector<std::pair<std::string,const Values*>>{{"E",&e_},{"M",&m_},{"W",&w_}})
            for(const std::string f:{"VALID","CONTROL","REGISTER_ADDRESSES"}) h[name+"_"+f]=get(*p,"IN_"+f);
        auto c=control({{"ENABLE",get(in,"ENABLE")},{"RST",get(in,"RST")},{"HALTED",halted_},{"TRAPPED",trapped_},
            {"DATA_HAZARD",get(dependency(h),"DATA_HAZARD")},{"D_TERMINAL",get(d,"TERMINAL")},{"E_TERMINAL",get(e,"TERMINAL")},
            {"M_TERMINAL",get(m,"TERMINAL")},{"W_TERMINAL",wt},{"D_CONTROL_FLOW",get(d,"CONTROL_FLOW")},{"E_CONTROL_FLOW",get(e,"CONTROL_FLOW")}});
        return {d,e,m,c};
    }
    static void capture(Values& dst,const Values& src,const std::vector<std::string>& fields,bool flush) {
        dst["IN_VALID"]=get(src,"OUT_VALID")&&!flush;for(auto& f:fields) dst["IN_"+f]=get(src,"OUT_"+f);
    }
    void advance(const Values& in) {
        const auto [d,e,m,c]=comb(in);if(!get(c,"ACTIVE")) return;
        // Compute all stages above from pre-edge state, then update together.
        if(get(w_,"IN_VALID")) {
            const auto ctrl=unpackControl(get(w_,"IN_CONTROL"));const auto trap=trapCause(w_);
            if(trap) {trapped_=true;cause_=trap;}else if(ctrl.halt) halted_=true;
            else {const auto dest=rd(get(w_,"IN_REGISTER_ADDRESSES"));if(ctrl.reg_write&&dest) regs_[dest]=wbValue(w_);}
        }
        Values f{{"OUT_VALID",get(c,"FETCH_VALID")},{"OUT_PC",fetch_pc_},{"OUT_INSTRUCTION",get(in,"IMEM_RSP_READ_DATA")},
            {"OUT_FETCH_STATUS",uint32_t(bool(fetch_pc_&3))|(uint32_t(get(in,"IMEM_RSP_STATUS")!=0)<<1)}};
        if(get(c,"PC_WRITE")) fetch_pc_=get(c,"PC_REDIRECT")?get(e,"OUT_NEXT_PC"):fetch_pc_+4;
        if(get(c,"IF_ID_WRITE")) capture(d_,f,fetchFields,get(c,"IF_ID_FLUSH"));
        capture(e_,d,decodeFields,get(c,"ID_EX_FLUSH"));capture(m_,e,executeFields,get(c,"EX_MEM_FLUSH"));capture(w_,m,memoryFields,get(c,"MEM_WB_FLUSH"));
    }
};
}
std::shared_ptr<Component> direct(Block b,const std::string& n,const std::shared_ptr<circuit::BuildContext>& c) {
    if(b==Block::DecodeStage) return Component::createWithContext<BehavioralDecode>(c,n);
    return Component::createWithContext<BehavioralStage>(c,n,b);
}
std::shared_ptr<Component> directCore(const std::string& n,const std::shared_ptr<circuit::BuildContext>& c) {
    return Component::createWithContext<BehavioralCore>(c,n);
}
}
