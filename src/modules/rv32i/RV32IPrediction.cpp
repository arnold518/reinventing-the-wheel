#include "modules/rv32i/RV32IPrediction.hpp"

#include "components/BasicComponent.hpp"
#include "components/selection/BuildContext.hpp"
#include "components/StructuralNetBuilder.hpp"
#include "modules/memory/FlipFlopCell.hpp"
#include "modules/memory/Register.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/composite/IncrementBy4.hpp"
#include "modules/rv32i/RV32IBitPatternMatcher.hpp"
#include "modules/utility/BitAdapter.hpp"
#include "modules/utility/Rewire.hpp"
#include <array>

namespace {
using circuit::detail::StructuralWiring;
using Ptr = std::shared_ptr<IOComponent>;
using Signal = std::shared_ptr<PinBase>;
using Policy = rv32i::PredictionPolicy;
void input(StructuralWiring& w, IOComponent& s, Ptr c, const std::string& p, const std::string& q = "") {
    w.connect(s.getInputPinDynamic(p), c->getInputPinDynamic(q.empty() ? p : q));
}
void output(StructuralWiring& w, IOComponent& s, Ptr c, const std::string& p, const std::string& q = "") {
    w.connect(c->getOutputPinDynamic(p), s.getOutputPinDynamic(q.empty() ? p : q));
}
Signal mux(ComponentBuilder& b, StructuralWiring& w, const std::string& name, Signal a, Signal d, Signal select) {
    Ptr c = a->getWidth() == 1 ? std::static_pointer_cast<IOComponent>(b.addNewComponent<Mux2to1>(name))
                             : std::static_pointer_cast<IOComponent>(b.addNewComponent<Mux2to1_32bit>(name));
    w.connect(a,c->getInputPinDynamic("A")); w.connect(d,c->getInputPinDynamic("B"));
    w.connect(select,c->getInputPin("SEL")); return c->getOutputPinDynamic("OUT");
}
Signal match(ComponentBuilder& b, StructuralWiring& w, const std::string& name, Signal value, uint32_t mask, uint32_t bits) {
    if (!mask) return w.constant(name,true);
    auto c = b.addNewComponent<RV32IBitPatternMatcher>(name,mask,bits);
    w.connect(value,c->getInputPin<32>("INPUT")); return c->getOutputPin("MATCH");
}
void counterPins(IOComponent* s) {
    s->addPin<2>("CURRENT",PinType::INPUT);
    for (auto p : {"TAKEN","ONE_BIT"}) s->addPin(p,PinType::INPUT);
    s->addPin<2>("NEXT",PinType::OUTPUT);
}
void predecodePins(IOComponent* s) {
    for (auto p : {"PC","INSTRUCTION"}) s->addPin<32>(p,PinType::INPUT);
    for (auto p : {"CONDITIONAL","DIRECT_JUMP","BACKWARD"}) s->addPin(p,PinType::OUTPUT);
    for (auto p : {"TARGET","PC_PLUS_4"}) s->addPin<32>(p,PinType::OUTPUT);
}
void predictorPins(IOComponent* s) {
    for (auto p : {"CLK","RST","CONDITIONAL","DIRECT_JUMP","BACKWARD","UPDATE","UPDATE_TAKEN"}) s->addPin(p,PinType::INPUT);
    for (auto p : {"PC","TARGET","PC_PLUS_4","UPDATE_PC"}) s->addPin<32>(p,PinType::INPUT);
    s->addPin<32>("NEXT_PC",PinType::OUTPUT); s->addPin("TAKEN",PinType::OUTPUT);
    s->addPin<2>("COUNTER",PinType::OUTPUT);
}
void recoveryPins(IOComponent* s) {
    for (auto p : {"VALID","PRETERMINAL","ADVANCE","KILL","IS_BRANCH","RST","TAKEN"}) s->addPin(p,PinType::INPUT);
    for (auto p : {"PREDICTED_PC","FALLTHROUGH_PC","TARGET_PC"}) s->addPin<32>(p,PinType::INPUT);
    for (auto p : {"MISPREDICT","RESOLVE","UPDATE","RECOVERY","BRANCH_MISS"}) s->addPin(p,PinType::OUTPUT);
}
void pathPins(IOComponent* s) {
    for (auto p : {"CLK","RST","IF_ID_WRITE","ID_EX_WRITE","EX_VALID","EX_PRETERMINAL","EX_ADVANCE",
                  "EX_KILL","EX_IS_BRANCH","EX_TAKEN","EX_CONTROL_TAKEN","REDIRECT_APPLY"}) s->addPin(p,PinType::INPUT);
    for (auto p : {"FETCH_PC","FETCH_INSTRUCTION","EX_PC","EX_FALLTHROUGH_PC","EX_TARGET_PC"}) s->addPin<32>(p,PinType::INPUT);
    for (auto p : {"FETCH_NEXT_PC","PREDICTED_NEXT_PC","EX_PREDICTED_NEXT_PC"}) s->addPin<32>(p,PinType::OUTPUT);
    for (auto p : {"MISPREDICT","RESOLVE","UPDATE","RECOVERY","BRANCH_MISS"}) s->addPin(p,PinType::OUTPUT);
}

// One row retains one or two predictor bits according to the selected policy.
class PredictionEntry : public IOComponent {
public:
    PredictionEntry(std::string name, bool one_bit) : IOComponent(std::move(name),[](IOComponent* s) {
        for (auto p : {"CLK","RST","WE","TAKEN"}) s->addPin(p,PinType::INPUT);
        s->addPin<2>("Q",PinType::OUTPUT);
    }), one_bit_(one_bit) {}
    const char* getTypeName() const override { return "RV32IPredictionEntry"; }
    void buildInternals(ComponentBuilder& b) override {
        StructuralWiring w(b);
        auto value = b.addNewComponent<BitJoiner<2>>("VALUE");
        w.connect(value->getOutputPin<2>("OUT"),getOutputPin<2>("Q"));
        if (one_bit_) {
            auto cell = b.add(circuit::families::FlipFlopCell,"BIT_0");
            for (auto p : {"CLK","RST","WE","TAKEN"})
                input(w,*this,cell,p,std::string(p)=="TAKEN" ? "D" : p);
            w.connect(cell->getOutputPin("Q"),value->getInputPin("IN_0"));
            w.connect(cell->getOutputPin("Q"),value->getInputPin("IN_1"));
            w.finish();
            return;
        }
        auto update = b.add(circuit::families::RV32IPredictionCounter,"UPDATE");
        auto bits = b.addNewComponent<BitSplitter<2>>("NEXT_BITS");
        input(w,*this,update,"TAKEN");
        w.connect(w.constant("ONE_BIT",false),update->getInputPin("ONE_BIT"));
        w.connect(update->getOutputPin<2>("NEXT"),bits->getInputPin<2>("IN"));
        w.connect(value->getOutputPin<2>("OUT"),update->getInputPin<2>("CURRENT"));
        for (unsigned bit=0;bit<2;++bit) {
            const auto n = std::to_string(bit);
            auto cell = b.add(circuit::families::FlipFlopCell,"BIT_"+n);
            for (auto p : {"CLK","RST","WE"}) input(w,*this,cell,p);
            w.connect(bits->getOutputPin("OUT_"+n),cell->getInputPin("D"));
            w.connect(cell->getOutputPin("Q"),value->getInputPin("IN_"+n));
        }
        w.finish();
    }
private: bool one_bit_;
};

class PredictionTable : public IOComponent {
public:
    PredictionTable(std::string name, rv32i::PredictionConfig config)
        : IOComponent(std::move(name),[](IOComponent* s) {
            for (auto p : {"CLK","RST","UPDATE","TAKEN"}) s->addPin(p,PinType::INPUT);
            for (auto p : {"PC","UPDATE_PC"}) s->addPin<32>(p,PinType::INPUT);
            s->addPin<2>("COUNTER",PinType::OUTPUT);
        }), config_(config), type_("RV32IPredictionTable<"+std::to_string(config.entries)+">") { config_.validate(); }
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder& b) override {
        StructuralWiring w(b);
        std::array<std::vector<Signal>,2> columns;
        for (size_t row=0; row<config_.entries; ++row) {
            const auto n=std::to_string(row);
            auto entry=b.addNewComponent<PredictionEntry>("ROW_"+n,config_.policy==Policy::OneBit);
            for (auto p : {"CLK","RST","TAKEN"}) input(w,*this,entry,p);
            auto selected=match(b,w,"UPDATE_ROW_"+n,getInputPin<32>("UPDATE_PC"),uint32_t((config_.entries-1)<<2),uint32_t(row<<2));
            w.connect(w.both("WRITE_ROW_"+n,selected,getInputPin("UPDATE")),entry->getInputPin("WE"));
            auto bits=b.addNewComponent<BitSplitter<2>>("ROW_BITS_"+n);
            w.connect(entry->getOutputPin<2>("Q"),bits->getInputPin<2>("IN"));
            for (unsigned bit=0;bit<2;++bit) columns[bit].push_back(bits->getOutputPin("OUT_"+std::to_string(bit)));
        }
        unsigned address_bits=0;
        while ((size_t{1}<<address_bits)<config_.entries) ++address_bits;
        std::vector<Rewire::WireSpec> outputs;
        std::vector<Rewire::BitMap> maps;
        for (unsigned bit=0;bit<address_bits;++bit) {
            outputs.emplace_back("BIT_"+std::to_string(bit),1);
            maps.emplace_back("PC",bit+2,outputs.back().name,0);
        }
        Ptr address;
        if (address_bits) {
            address=b.addNewComponent<Rewire>("LOOKUP_INDEX",std::vector<Rewire::WireSpec>{{"PC",32}},outputs,maps);
            input(w,*this,address,"PC");
        }
        auto result=b.addNewComponent<BitJoiner<2>>("READ_COUNTER");
        for (unsigned bit=0;bit<2;++bit) {
            if (config_.policy == Policy::OneBit && bit == 1) continue;
            auto values=columns[bit];
            for (unsigned level=0;values.size()>1;++level) {
                std::vector<Signal> next;
                for (size_t i=0;i<values.size();i+=2)
                    next.push_back(mux(b,w,"READ_"+std::to_string(bit)+"_"+std::to_string(level)+"_"+std::to_string(i/2),
                        values[i],values[i+1],address->getOutputPin("BIT_"+std::to_string(level))));
                values=std::move(next);
            }
            w.connect(values.front(),result->getInputPin("IN_"+std::to_string(bit)));
            if (config_.policy == Policy::OneBit)
                w.connect(values.front(),result->getInputPin("IN_1"));
        }
        output(w,*this,result,"OUT","COUNTER"); w.finish();
    }
private: rv32i::PredictionConfig config_; std::string type_;
};

enum class Kind { Counter, Predecode, Predictor, Recovery, Path };
class PredictionDirect : public BasicComponent {
public:
    PredictionDirect(std::string name, Kind kind, const circuit::ComponentFamily& family, rv32i::PredictionConfig config = {})
        : BasicComponent(std::move(name),1,family.pinInitializer()), kind_(kind), type_(family.typeName()), state_(config) {}
    const char* getTypeName() const override { return type_.c_str(); }
    void evaluate(size_t time, Simulator& sim) override {
        auto n=[&](const char* p){return uint32_t(getInputPinDynamic(p)->getValueAsUInt64());};
        auto h=[&](const char* p){return getInputValue(p)==LogicValue::HIGH;};
        auto bit=[&](const char* p,bool value){_updateOutputWire(sim,p,value?LogicValue::HIGH:LogicValue::LOW,time);};
        auto word=[&](const char* p,uint32_t value){_updateOutputWire<32>(sim,p,value,time);};
        if (kind_==Kind::Counter) {
            _updateOutputWire<2>(sim,"NEXT",rv32i::nextPredictionCounter(n("CURRENT"),h("TAKEN"),h("ONE_BIT")),time); return;
        }
        if (kind_==Kind::Predecode) {
            const auto d=rv32i::predecodeBranch(n("PC"),n("INSTRUCTION"));
            bit("CONDITIONAL",d.conditional);bit("DIRECT_JUMP",d.direct_jump);bit("BACKWARD",d.backward);
            word("TARGET",d.target);word("PC_PLUS_4",d.fallthrough);return;
        }
        if (kind_==Kind::Recovery) {
            const bool eligible=h("VALID")&&!h("PRETERMINAL")&&!h("RST");
            const bool resolve=eligible&&h("ADVANCE")&&!h("KILL");
            const auto actual=n(h("TAKEN")?"TARGET_PC":"FALLTHROUGH_PC");
            bit("MISPREDICT",eligible&&n("PREDICTED_PC")!=actual);
            bit("RESOLVE",resolve);bit("UPDATE",resolve&&h("IS_BRANCH"));
            bit("RECOVERY",resolve&&n("PREDICTED_PC")!=actual);
            bit("BRANCH_MISS",resolve&&h("IS_BRANCH")&&n("PREDICTED_PC")!=actual);return;
        }
        const auto clk=getInputValue("CLK");
        const bool edge=previous_==LogicValue::LOW&&clk==LogicValue::HIGH;
        if (h("RST")) {state_.reset();if_id_=id_ex_=0;}
        else if (edge) {
            if (kind_==Kind::Predictor) {
                if (h("UPDATE")) state_.update(n("UPDATE_PC"),h("UPDATE_TAKEN"));
            } else {
                // All capture values are from pre-edge state, including a lookup
                // colliding with the table's update address.
                const auto predicted=state_.predict(n("FETCH_PC"),n("FETCH_INSTRUCTION"));
                const auto old_if_id=if_id_;
                if (h("IF_ID_WRITE")) if_id_=predicted;
                if (h("ID_EX_WRITE")) id_ex_=old_if_id;
                if (h("EX_VALID")&&!h("EX_PRETERMINAL")&&h("EX_ADVANCE")&&!h("EX_KILL")&&h("EX_IS_BRANCH"))
                    state_.update(n("EX_PC"),h("EX_TAKEN"));
            }
        }
        previous_=clk;
        if (kind_==Kind::Predictor) {
            const bool taken=h("DIRECT_JUMP")||(h("CONDITIONAL")&&state_.direction(n("PC"),h("BACKWARD")));
            bit("TAKEN",taken); word("NEXT_PC",n(taken?"TARGET":"PC_PLUS_4"));
            _updateOutputWire<2>(sim,"COUNTER",state_.counter(n("PC")),time);
        } else {
            const auto predicted=state_.predict(n("FETCH_PC"),n("FETCH_INSTRUCTION"));
            word("PREDICTED_NEXT_PC",predicted);word("EX_PREDICTED_NEXT_PC",id_ex_);
            const auto actual=n(h("EX_CONTROL_TAKEN")?"EX_TARGET_PC":"EX_FALLTHROUGH_PC");
            word("FETCH_NEXT_PC",h("REDIRECT_APPLY")?actual:predicted);
            const bool eligible=h("EX_VALID")&&!h("EX_PRETERMINAL")&&!h("RST");
            const bool resolve=eligible&&h("EX_ADVANCE")&&!h("EX_KILL");
            bit("MISPREDICT",eligible&&id_ex_!=actual);
            bit("RESOLVE",resolve);bit("UPDATE",resolve&&h("EX_IS_BRANCH"));
            bit("RECOVERY",resolve&&id_ex_!=actual);
            bit("BRANCH_MISS",resolve&&h("EX_IS_BRANCH")&&id_ex_!=actual);
        }
    }
private:
    Kind kind_; std::string type_; rv32i::PredictionState state_;
    LogicValue previous_=LogicValue::UNKNOWN;
    uint32_t if_id_=0,id_ex_=0;
};
rv32i::PredictionConfig configOf(const std::shared_ptr<circuit::BuildContext>& c) {
    return c ? rv32i::predictionConfig(c->parameters()) : rv32i::PredictionConfig{};
}
}

#define DEFINE_FIXED(Name,Id,Pins,KindName) \
namespace circuit::families { const ComponentFamily Name{ "rv32i.prediction." Id,#Name,Pins, \
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::Name>(c,n);}, \
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<PredictionDirect>(c,n,Kind::KindName,Name);} }; } \
Name::Name(std::string n):IOComponent(std::move(n),Pins) {}
DEFINE_FIXED(RV32IPredictionCounter,"counter",counterPins,Counter)
DEFINE_FIXED(RV32IBranchPredecode,"predecode",predecodePins,Predecode)
DEFINE_FIXED(RV32IBranchRecovery,"recovery",recoveryPins,Recovery)
#undef DEFINE_FIXED
#define DEFINE_CONFIGURED(Name,Id,Pins,KindName) \
namespace circuit::families { const ComponentFamily Name{ "rv32i.prediction." Id,#Name,Pins, \
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<::Name>(c,n,configOf(c));}, \
    [](const std::string& n,const std::shared_ptr<BuildContext>& c){return Component::createWithContext<PredictionDirect>(c,n,Kind::KindName,Name,configOf(c));} }; } \
Name::Name(std::string n,rv32i::PredictionConfig config):IOComponent(std::move(n),Pins),config_(config) { config_.validate(); }
DEFINE_CONFIGURED(RV32IBranchPredictor,"predictor",predictorPins,Predictor)
DEFINE_CONFIGURED(RV32IPredictionPath,"path",pathPins,Path)
#undef DEFINE_CONFIGURED

void RV32IPredictionCounter::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto bits=b.addNewComponent<BitSplitter<2>>("STATE_BITS");
    auto join=b.addNewComponent<BitJoiner<2>>("NEXT_STATE");
    input(w,*this,bits,"CURRENT","IN");
    auto low=bits->getOutputPin("OUT_0"),high=bits->getOutputPin("OUT_1");
    auto no_low=w.invert("NOT_LOW",low);
    std::array<Signal,2> up{w.either("UP_LOW",high,no_low),w.either("UP_HIGH",high,low)};
    std::array<Signal,2> down{w.both("DOWN_LOW",high,no_low),w.both("DOWN_HIGH",high,low)};
    for (unsigned bit=0;bit<2;++bit) {
        const auto n=std::to_string(bit);
        auto next=mux(b,w,"DIRECTION_"+n,down[bit],up[bit],getInputPin("TAKEN"));
        w.connect(mux(b,w,"MODE_"+n,next,getInputPin("TAKEN"),getInputPin("ONE_BIT")),join->getInputPin("IN_"+n));
    }
    output(w,*this,join,"OUT","NEXT");w.finish();
}

void RV32IBranchPredecode::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto instruction=getInputPin<32>("INSTRUCTION");
    auto opcode=match(b,w,"BRANCH_OPCODE",instruction,0x7f,0x63);
    auto reserved=match(b,w,"RESERVED_BRANCH",instruction,0x6000,0x2000);
    auto branch=w.both("CONDITIONAL",opcode,w.invert("VALID_FUNCTION",reserved));
    auto jump=match(b,w,"JAL_OPCODE",instruction,0x7f,0x6f);
    w.connect(branch,getOutputPin("CONDITIONAL"));w.connect(jump,getOutputPin("DIRECT_JUMP"));
    std::vector<Rewire::BitMap> maps;
    auto copy=[&](unsigned first,unsigned count,const char* out,unsigned target) {
        auto m=identity_mapping("INSTRUCTION",first,count,out,target);maps.insert(maps.end(),m.begin(),m.end());
    };
    copy(8,4,"B",1);copy(25,6,"B",5);copy(7,1,"B",11);
    copy(21,10,"J",1);copy(20,1,"J",11);copy(12,8,"J",12);
    for (unsigned bit=12;bit<32;++bit) maps.emplace_back("INSTRUCTION",31,"B",bit);
    for (unsigned bit=20;bit<32;++bit) maps.emplace_back("INSTRUCTION",31,"J",bit);
    maps.emplace_back("INSTRUCTION",31,"SIGN",0);
    auto fields=b.addNewComponent<Rewire>("IMMEDIATES",std::vector<Rewire::WireSpec>{{"INSTRUCTION",32}},
        std::vector<Rewire::WireSpec>{{"B",32},{"J",32},{"SIGN",1}},maps,Rewire::UnmappedBitValue::LOW);
    input(w,*this,fields,"INSTRUCTION");
    auto zero=b.addNewComponent<ConstantValue<32>>("ZERO",0);
    auto immediate=mux(b,w,"BRANCH_IMMEDIATE",zero->getOutputPin<32>("OUT"),fields->getOutputPin<32>("B"),branch);
    immediate=mux(b,w,"JUMP_IMMEDIATE",immediate,fields->getOutputPin<32>("J"),jump);
    auto target=b.addNewComponent<Adder32>("TARGET_ADD");
    auto sequential=b.addNewComponent<IncrementBy4>("SEQUENTIAL_ADD");
    input(w,*this,target,"PC","A");
    w.connect(w.constant("TARGET_CARRY",false),target->getInputPin("Cin"));
    input(w,*this,sequential,"PC","A");
    w.connect(immediate,target->getInputPin<32>("B"));
    output(w,*this,target,"Sum","TARGET");output(w,*this,sequential,"Sum","PC_PLUS_4");
    w.connect(w.both("BACKWARD",w.either("CONTROL_FLOW",branch,jump),fields->getOutputPin("SIGN")),getOutputPin("BACKWARD"));
    w.finish();
}

void RV32IBranchPredictor::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    Signal direction;
    if (config_.policy==Policy::OneBit||config_.policy==Policy::TwoBit) {
        auto table=b.addNewComponent<PredictionTable>("TABLE",config_);
        for (auto p : {"CLK","RST","PC","UPDATE","UPDATE_PC"}) input(w,*this,table,p);
        input(w,*this,table,"UPDATE_TAKEN","TAKEN");
        output(w,*this,table,"COUNTER");
        auto bits=b.addNewComponent<BitSplitter<2>>("COUNTER_BITS");
        w.connect(table->getOutputPin<2>("COUNTER"),bits->getInputPin<2>("IN"));
        direction=bits->getOutputPin("OUT_1");
    } else {
        auto zero=b.addNewComponent<ConstantValue<2>>("NO_TABLE",0);
        output(w,*this,zero,"OUT","COUNTER");
        direction=config_.policy==Policy::BackwardTaken ? getInputPin("BACKWARD")
            : w.constant("STATIC_DIRECTION",config_.policy==Policy::Taken);
    }
    auto taken=w.either("TAKEN",getInputPin("DIRECT_JUMP"),w.both("BRANCH_DIRECTION",getInputPin("CONDITIONAL"),direction));
    w.connect(taken,getOutputPin("TAKEN"));
    w.connect(mux(b,w,"NEXT_PC_SELECT",getInputPin<32>("PC_PLUS_4"),getInputPin<32>("TARGET"),taken),getOutputPin<32>("NEXT_PC"));
    w.finish();
}

void RV32IBranchRecovery::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto a=b.addNewComponent<BitSplitter<32>>("PREDICTED_BITS");
    input(w,*this,a,"PREDICTED_PC","IN");
    // Compare both candidate addresses in parallel; the late branch condition
    // selects one bit, not a 32-bit word followed by a comparison tree.
    const auto compare=[&](const std::string& name,const std::string& pin) {
        auto d=b.addNewComponent<BitSplitter<32>>(name+"_BITS");
        input(w,*this,d,pin,"IN");
        std::vector<Signal> differences;
        for(unsigned bit=0;bit<32;++bit) {
            const auto n=std::to_string(bit);
            differences.push_back(w.binary<XORGate>(name+"_DIFFERENT_"+n,a->getOutputPin("OUT_"+n),d->getOutputPin("OUT_"+n)));
        }
        return w.reduce(name+"_DIFFERENCE",differences,false);
    };
    // Component creation order must not depend on C++ argument evaluation
    // order (GCC/native and Clang/WASM can choose opposite orders).
    auto fallthrough_difference=compare("FALLTHROUGH","FALLTHROUGH_PC");
    auto target_difference=compare("TARGET","TARGET_PC");
    auto different=mux(b,w,"MISMATCH_SELECT",fallthrough_difference,
        target_difference,getInputPin("TAKEN"));
    auto eligible=w.reduce("ELIGIBLE",{getInputPin("VALID"),w.invert("NOT_TERMINAL",getInputPin("PRETERMINAL")),
                                                       w.invert("NOT_RESET",getInputPin("RST"))},true);
    auto resolve=w.reduce("RESOLVE",{eligible,getInputPin("ADVANCE"),w.invert("NOT_KILLED",getInputPin("KILL"))},true);
    w.connect(w.both("MISPREDICT",eligible,different),getOutputPin("MISPREDICT"));
    auto recovery=w.both("RECOVERY",resolve,different);
    w.connect(recovery,getOutputPin("RECOVERY"));
    w.connect(w.both("BRANCH_MISS",recovery,getInputPin("IS_BRANCH")),getOutputPin("BRANCH_MISS"));
    w.connect(resolve,getOutputPin("RESOLVE"));
    w.connect(w.both("TRAIN_BRANCH",resolve,getInputPin("IS_BRANCH")),getOutputPin("UPDATE"));w.finish();
}

void RV32IPredictionPath::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto predecode=b.add(circuit::families::RV32IBranchPredecode,"PREDECODE");
    Ptr predictor=b.getBuildContext() ? b.add(circuit::families::RV32IBranchPredictor,"PREDICTOR",config_.parameters())
        : b.addNewComponent<RV32IBranchPredictor>("PREDICTOR",config_);
    auto if_id=b.add(circuit::families::registerWord(32),"IF_ID_PREDICTION");
    auto id_ex=b.add(circuit::families::registerWord(32),"ID_EX_PREDICTION");
    auto recovery=b.add(circuit::families::RV32IBranchRecovery,"RECOVERY");
    input(w,*this,predecode,"FETCH_PC","PC");input(w,*this,predecode,"FETCH_INSTRUCTION","INSTRUCTION");
    input(w,*this,predictor,"FETCH_PC","PC");
    for (auto p : {"CONDITIONAL","DIRECT_JUMP","BACKWARD","TARGET","PC_PLUS_4"})
        w.connect(predecode->getOutputPinDynamic(p),predictor->getInputPinDynamic(p));
    for (auto c : {predictor,if_id,id_ex}) for (auto p : {"CLK","RST"}) input(w,*this,c,p);
    input(w,*this,if_id,"IF_ID_WRITE","WE");input(w,*this,id_ex,"ID_EX_WRITE","WE");
    w.connect(predictor->getOutputPin<32>("NEXT_PC"),if_id->getInputPin<32>("D"));
    w.connect(if_id->getOutputPin<32>("Q"),id_ex->getInputPin<32>("D"));
    output(w,*this,predictor,"NEXT_PC","PREDICTED_NEXT_PC");output(w,*this,id_ex,"Q","EX_PREDICTED_NEXT_PC");
    w.connect(id_ex->getOutputPin<32>("Q"),recovery->getInputPin<32>("PREDICTED_PC"));
    input(w,*this,recovery,"EX_FALLTHROUGH_PC","FALLTHROUGH_PC");
    input(w,*this,recovery,"EX_TARGET_PC","TARGET_PC");
    input(w,*this,recovery,"EX_CONTROL_TAKEN","TAKEN");input(w,*this,recovery,"RST");
    for (auto p : {"VALID","PRETERMINAL","ADVANCE","KILL","IS_BRANCH"}) input(w,*this,recovery,std::string("EX_")+p,p);
    for (auto p : {"MISPREDICT","RESOLVE","UPDATE","RECOVERY","BRANCH_MISS"}) output(w,*this,recovery,p);
    w.connect(recovery->getOutputPin("UPDATE"),predictor->getInputPin("UPDATE"));
    input(w,*this,predictor,"EX_PC","UPDATE_PC");input(w,*this,predictor,"EX_TAKEN","UPDATE_TAKEN");
    auto actual=mux(b,w,"ACTUAL_NEXT_PC",getInputPin<32>("EX_FALLTHROUGH_PC"),getInputPin<32>("EX_TARGET_PC"),getInputPin("EX_CONTROL_TAKEN"));
    w.connect(mux(b,w,"RECOVERY_PC_SELECT",predictor->getOutputPin<32>("NEXT_PC"),actual,getInputPin("REDIRECT_APPLY")),getOutputPin<32>("FETCH_NEXT_PC"));
    w.finish();
}
