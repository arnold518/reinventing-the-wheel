#include "RV32ISingleCycleStagesDirect.hpp"
#include "modules/composite/ALU32Direct.hpp"
#include "RV32IMemoryRequestModel.hpp"
#include "components/BasicComponent.hpp"
#include "components/Component.tpp"
#include "modules/composite/ALU32.hpp"
#include "modules/rv32i/RV32IComponentEncoding.hpp"
#include "rv32i/RV32IControl.hpp"
#include "rv32i/RV32IDecoder.hpp"
#include <map>
#include <optional>

namespace rv32i::single_cycle {
namespace {
using Bits=std::vector<LogicValue>;
Bits bits(size_t width,uint32_t value) {
    Bits r(width);for(size_t i=0;i<width;++i) r[i]=(value>>i)&1?LogicValue::HIGH:LogicValue::LOW;
    return r;
}
// Independent compact reference for the stage's settled binary/edge contract.
// It never instantiates a shadow structural circuit or executes an instruction
// oracle. In particular, Execute has no state, and Decode writes only on CLK.
class DirectStage : public BasicComponent {
public:
    DirectStage(std::string name,Stage stage)
        : BasicComponent(std::move(name),1,family(stage).pinInitializer()),stage_(stage) {
        if(stage==Stage::Decode) {
            registers_.assign(32,Bits(32,LogicValue::UNKNOWN));registers_[0]=bits(32,0);
            history_[0]=registers_;
        }
    }
    const char* getTypeName() const override {return family(stage_).typeName().data();}
    std::vector<Bits> registerStateAtTime(size_t time) const {
        if(history_.empty()) return {};
        auto it=history_.upper_bound(time);
        return it==history_.begin()?it->second:std::prev(it)->second;
    }
    void evaluate(size_t time,Simulator& sim) override {
        switch(stage_) {
            case Stage::Fetch:fetch(time,sim);break;
            case Stage::Decode:decode(time,sim);break;
            case Stage::Execute:execute(time,sim);break;
            case Stage::Memory:memory(time,sim);break;
            case Stage::Writeback: {
                auto sel=known("WRITEBACK_SEL");
                if(!sel) emit("WB_VALUE",Bits(32,LogicValue::UNKNOWN),time,sim);
                else if(*sel==0) emit("WB_VALUE",0,time,sim);
                else emit("WB_VALUE",getInputPinDynamic(*sel==1?"ALU_RESULT":*sel==2?"READ_DATA":"PC_PLUS_4")->getValueAsVector(),time,sim);
                break;
            }
            case Stage::NextPC: {
                if(!known("JUMP_TYPE")||!known("BRANCH_TAKEN")) {unknownOutputs(time,sim);break;}
                const bool taken=value("JUMP_TYPE")||high("BRANCH_TAKEN");
                const auto source=taken?"CONTROL_TARGET":"PC_PLUS_4";
                emit("NEXT_PC",getInputPinDynamic(source)->getValueAsVector(),time,sim);
                if(!taken) emit("TARGET_MISALIGNED",0,time,sim);
                else if(auto target=known("CONTROL_TARGET")) emit("TARGET_MISALIGNED",bool(*target&3),time,sim);
                else emit("TARGET_MISALIGNED",Bits(1,LogicValue::UNKNOWN),time,sim);
                break;
            }
        }
    }
private:
    Stage stage_;
    uint32_t pc_=0;
    LogicValue previous_clock_=LogicValue::UNKNOWN;
    std::vector<Bits> registers_;
    std::map<size_t,std::vector<Bits>> history_;
    std::optional<uint32_t> known(const std::string& name) const {
        uint32_t result=0;const auto b=getInputPinDynamic(name)->getValueAsVector();
        for(size_t i=0;i<b.size();++i) {
            if(b[i]!=LogicValue::HIGH&&b[i]!=LogicValue::LOW) return {};
            if(b[i]==LogicValue::HIGH) result|=uint32_t{1}<<i;
        }
        return result;
    }
    uint32_t value(const std::string& name) const {return known(name).value_or(0);}
    bool high(const std::string& name) const {return known(name)==1;}
    bool edge() {
        auto clk=getInputPin("CLK")->getValue();
        bool rise=previous_clock_==LogicValue::LOW&&clk==LogicValue::HIGH;
        previous_clock_=clk;return rise;
    }
    void emit(const std::string& name,uint32_t value,size_t t,Simulator& s) {
        emit(name,bits(getOutputPinDynamic(name)->getWidth(),value),t,s);
    }
    void emit(const std::string& name,const Bits& value,size_t t,Simulator& s) {
        switch(value.size()) {
            case 1:_updateOutputWire<1>(s,name,value,t);break;
            case 2:_updateOutputWire<2>(s,name,value,t);break;
            case 3:_updateOutputWire<3>(s,name,value,t);break;
            case 4:_updateOutputWire<4>(s,name,value,t);break;
            case 5:_updateOutputWire<5>(s,name,value,t);break;
            case 32:_updateOutputWire<32>(s,name,value,t);break;
            default:throw std::logic_error("Unsupported single-cycle stage output width");
        }
    }
    void unknownOutputs(size_t t,Simulator& s) {
        for(auto& [name,pin]:getAllOutputPins()) emit(name,Bits(pin->getWidth(),LogicValue::UNKNOWN),t,s);
    }
    void fetch(size_t t,Simulator& s) {
        const bool rising=edge(),reset=high("RST");
        if(reset) pc_=0;
        else if(rising&&high("PC_WRITE")) pc_=value("NEXT_PC");
        emit("PC",pc_,t,s);emit("PC_PLUS_4",pc_+4,t,s);
        emit("PC_MISALIGNED",bool(pc_&3),t,s);
        emit("INSTRUCTION",getInputPin<32>("IMEM_READ_DATA")->getValueAsVector(),t,s);
        emit("IMEM_WORD_ADDR",pc_>>2,t,s);emit("IMEM_READ_EN",high("ISSUE"),t,s);
        emit("DONE",1,t,s);emit("FAULT",getInputPin("IMEM_FAULT")->getValueAsVector(),t,s);
    }
    void decode(size_t t,Simulator& s) {
        const bool rising=edge();
        auto raw=known("INSTRUCTION");
        const auto d=RV32IDecoder::decode(raw.value_or(0));
        auto old=registers_;
        if(high("RST")) for(auto& reg:registers_) reg=bits(32,0);
        else if(rising&&high("REGISTER_WRITE")&&raw&&d.rd!=0)
            registers_[d.rd]=getInputPin<32>("WB_VALUE")->getValueAsVector();
        if(old!=registers_) history_[t+delay]=registers_;
        if(!raw) {unknownOutputs(t,s);return;}
        const auto c=RV32IControl::fromDecoded(d);
        using namespace component_encoding;
        const std::pair<const char*,uint32_t> values[]{
            {"RS1_ADDR",d.rs1},{"RS2_ADDR",d.rs2},{"RD_ADDR",d.rd},{"IMM",uint32_t(d.immediate)},
            {"ALU_OP",c.alu_op},{"ALU_A_SEL",aluSourceA(c.alu_a)},{"ALU_B_SEL",aluSourceB(c.alu_b)},
            {"LEGAL",c.legal},{"REG_WRITE",c.reg_write},{"MEM_READ",c.mem_read},{"MEM_WRITE",c.mem_write},
            {"WRITEBACK_SEL",writeback(c.writeback)},{"MEM_SIZE",memorySize(c.mem_size)},
            {"LOAD_SIGN_EXTEND",c.load_sign_extend},{"USES_RS1",c.uses_rs1},{"USES_RS2",c.uses_rs2},
            {"BRANCH_TYPE",branch(c.branch)},{"JUMP_TYPE",jump(c.jump)},
            {"HALT_REQUEST",c.halt},{"TRAP_REQUEST",c.trap},{"DECODE_TRAP_CAUSE",decodeTrapCause(c.trap_cause)}};
        for(auto& [name,v]:values) emit(name,v,t,s);
        emit("RS1_VALUE",registers_[d.rs1],t,s);emit("RS2_VALUE",registers_[d.rs2],t,s);
    }
    void execute(size_t t,Simulator& s) {
        for(auto& [name,pin]:getAllInputPins()) if(!known(name)) {unknownOutputs(t,s);return;}
        const auto as=value("ALU_A_SEL"),bs=value("ALU_B_SEL");
        const auto a=as==0?value("RS1_VALUE"):as==1?value("PC"):0;
        const auto b=bs==0?value("RS2_VALUE"):bs==1?value("IMM"):0;
        const auto op=value("ALU_OP"),result=alu32Result(op,a,b),jump=value("JUMP_TYPE");
        const bool cmp=op==ALU32Op::SUB||op==ALU32Op::SLT||op==ALU32Op::SLTU;
        const bool eq=cmp&&a==b,lt=cmp&&int32_t(a)<int32_t(b),ltu=cmp&&a<b;
        const bool conditions[]{false,eq,!eq,lt,!lt,ltu,!ltu,false};
        emit("ALU_RESULT",result,t,s);emit("BRANCH_TAKEN",conditions[value("BRANCH_TYPE")],t,s);
        emit("CONTROL_TARGET",jump==0?value("PC")+value("IMM"):jump==1?result:jump==2?result&~uint32_t{1}:0,t,s);
    }
    void memory(size_t t,Simulator& s) {
        const auto addr=value("ADDR"),size=value("SIZE");
        const bool bad=size==3||(size==1&&(addr&1))||(size==2&&(addr&3));
        emit("MISALIGNED",bad,t,s);emit("DONE",1,t,s);
        emit("FAULT",getInputPin("DMEM_FAULT")->getValueAsVector(),t,s);
        emit("DMEM_WORD_ADDR",addr>>2,t,s);emit("DMEM_READ_EN",high("ISSUE"),t,s);
        emit("DMEM_WE",high("ISSUE")&&high("MEM_WRITE")&&high("COMMIT")&&!bad,t,s);
        emit("DMEM_WRITE_DATA",value("WRITE_DATA")<<((addr&3)*8),t,s);
        emit("DMEM_WRITE_MASK",memoryWriteMask(addr,size),t,s);
        // Preserve unknown neighboring lanes without rejecting a known LB/LH.
        const auto word=getInputPin<32>("DMEM_READ_DATA")->getValueAsVector();
        Bits shifted(32,LogicValue::LOW),loaded(32,LogicValue::LOW);
        const size_t offset=(addr&3)*8;
        for(size_t i=0;i+offset<32;++i) shifted[i]=word[i+offset];
        const size_t width=size==0?8:size==1?16:size==2?32:0;
        for(size_t i=0;i<width;++i) loaded[i]=shifted[i];
        if(width&&high("SIGN_EXTEND")) for(size_t i=width;i<32;++i) loaded[i]=loaded[width-1];
        emit("READ_DATA",loaded,t,s);
    }
};
class DirectDecodeStage final : public DirectStage, public RegisterStateView {
public:
    explicit DirectDecodeStage(std::string name):DirectStage(std::move(name),Stage::Decode) {}
    std::vector<Bits> getRegisterStateAtTime(size_t time) const override { return registerStateAtTime(time); }
};
}
std::shared_ptr<IOComponent> directStage(Stage stage,const std::string& name,
    const std::shared_ptr<circuit::BuildContext>& context) {
    if(stage==Stage::Decode) return Component::createWithContext<DirectDecodeStage>(context,name);
    return Component::createWithContext<DirectStage>(context,name,stage);
}
}
