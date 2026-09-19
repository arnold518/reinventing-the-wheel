#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "RV32ISingleCycleStagesDirect.hpp"
#include "RV32IMemoryRequestWiring.hpp"
#include "modules/memory/Register32.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/composite/ALU32.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/utility/Rewire.hpp"
#include <stdexcept>

using circuit::memory_detail::StructuralWiring;
namespace {
void fetchPins(IOComponent* s) {
    for(auto p:{"CLK","RST","ISSUE","TAKE","PC_WRITE"}) s->addPin(p,PinType::INPUT);
    s->addPin<32>("NEXT_PC",PinType::INPUT);
    rv32i::memoryRequestPins(s,"IMEM_");
    for(auto p:{"PC","PC_PLUS_4","INSTRUCTION"}) s->addPin<32>(p,PinType::OUTPUT);
    for(auto p:{"DONE","FAULT","PC_MISALIGNED"}) s->addPin(p,PinType::OUTPUT);
}
void decodePins(IOComponent* s) {
    circuit::families::RV32IDecodeControl.pinInitializer()(s);
    for(auto p:{"CLK","RST","REGISTER_WRITE"}) s->addPin(p,PinType::INPUT);
    s->addPin<32>("WB_VALUE",PinType::INPUT);
    for(auto p:{"RS1_VALUE","RS2_VALUE"}) s->addPin<32>(p,PinType::OUTPUT);
}
void executePins(IOComponent* s) {
    for(auto p:{"PC","RS1_VALUE","RS2_VALUE","IMM"}) s->addPin<32>(p,PinType::INPUT);
    s->addPin<5>("ALU_OP",PinType::INPUT);
    for(auto p:{"ALU_A_SEL","ALU_B_SEL","JUMP_TYPE"}) s->addPin<2>(p,PinType::INPUT);
    s->addPin<3>("BRANCH_TYPE",PinType::INPUT);
    for(auto p:{"ALU_RESULT","CONTROL_TARGET"}) s->addPin<32>(p,PinType::OUTPUT);
    s->addPin("BRANCH_TAKEN",PinType::OUTPUT);
}
void memoryPins(IOComponent* s) {
    for(auto p:{"CLK","RST","ISSUE","TAKE","MEM_WRITE","SIGN_EXTEND"}) s->addPin(p,PinType::INPUT);
    for(auto p:{"ADDR","WRITE_DATA"}) s->addPin<32>(p,PinType::INPUT);
    s->addPin<2>("SIZE",PinType::INPUT);
    rv32i::memoryRequestPins(s,"DMEM_");
    s->addPin<32>("READ_DATA",PinType::OUTPUT);
    for(auto p:{"DONE","FAULT","MISALIGNED"}) s->addPin(p,PinType::OUTPUT);
}
void writebackPins(IOComponent* s) {
    for(auto p:{"ALU_RESULT","READ_DATA","PC_PLUS_4"}) s->addPin<32>(p,PinType::INPUT);
    s->addPin<2>("WRITEBACK_SEL",PinType::INPUT);
    s->addPin<32>("WB_VALUE",PinType::OUTPUT);
}
void nextPCPins(IOComponent* s) {
    for(auto p:{"PC_PLUS_4","CONTROL_TARGET"}) s->addPin<32>(p,PinType::INPUT);
    s->addPin("BRANCH_TAKEN",PinType::INPUT);
    s->addPin<2>("JUMP_TYPE",PinType::INPUT);
    s->addPin<32>("NEXT_PC",PinType::OUTPUT);
    s->addPin("TARGET_MISALIGNED",PinType::OUTPUT);
}
// Fanout must be collected once per scope; these helpers create wires only.
void in(StructuralWiring& w,IOComponent& s,const std::shared_ptr<IOComponent>& c,
        const std::string& p,const std::string& q="") {
    w.connect(s.getInputPinDynamic(p),c->getInputPinDynamic(q.empty()?p:q));
}
void out(StructuralWiring& w,IOComponent& s,const std::shared_ptr<IOComponent>& c,
         const std::string& p,const std::string& q="") {
    w.connect(c->getOutputPinDynamic(p),s.getOutputPinDynamic(q.empty()?p:q));
}
}

#define DEFINE_STAGE(Name,Id,Pins) \
namespace circuit::families { \
const ComponentFamily RV32ISingleCycle##Name##Stage{ \
    "rv32i.single-cycle." Id, "RV32ISingleCycle" #Name "Stage", Pins, \
    [](const std::string& n,const std::shared_ptr<BuildContext>& c) { \
        return Component::createWithContext<::RV32ISingleCycle##Name##Stage>(c,n); }, \
    [](const std::string& n,const std::shared_ptr<BuildContext>& c) { \
        return rv32i::single_cycle::directStage(rv32i::single_cycle::Stage::Name,n,c); }}; } \
RV32ISingleCycle##Name##Stage::RV32ISingleCycle##Name##Stage(std::string n) \
    : IOComponent(std::move(n),Pins) {}
DEFINE_STAGE(Fetch,"fetch",fetchPins)
DEFINE_STAGE(Decode,"decode",decodePins)
DEFINE_STAGE(Execute,"execute",executePins)
DEFINE_STAGE(Memory,"memory",memoryPins)
DEFINE_STAGE(Writeback,"writeback",writebackPins)
DEFINE_STAGE(NextPC,"next-pc",nextPCPins)
#undef DEFINE_STAGE

namespace rv32i::single_cycle {
const circuit::ComponentFamily& family(Stage s) {
    using namespace circuit::families;
    switch(s) {
        case Stage::Fetch:return circuit::families::RV32ISingleCycleFetchStage;
        case Stage::Decode:return circuit::families::RV32ISingleCycleDecodeStage;
        case Stage::Execute:return circuit::families::RV32ISingleCycleExecuteStage;
        case Stage::Memory:return circuit::families::RV32ISingleCycleMemoryStage;
        case Stage::Writeback:return circuit::families::RV32ISingleCycleWritebackStage;
        case Stage::NextPC:return circuit::families::RV32ISingleCycleNextPCStage;
    }
    throw std::logic_error("Invalid single-cycle stage");
}
}

void RV32ISingleCycleFetchStage::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto pc=b.add(circuit::families::Register32,"PC_STATE");
    auto add=b.addNewComponent<Adder32>("PC_PLUS_4_ADD");
    auto four=b.addNewComponent<ConstantValue<32>>("FOUR",4);
    auto request=b.addNewComponent<RV32IMemoryRequest>("REQUEST");
    auto align=b.add(circuit::families::RV32IMemoryAlignmentUnit,"ALIGNMENT");
    auto word=b.addNewComponent<ConstantValue<2>>("WORD_SIZE",2);
    auto zero=w.constant("ZERO",false);
    auto read=b.addNewComponent<ConstantValue<3>>("READ_OP",0);
    auto data=b.addNewComponent<ConstantValue<32>>("WRITE_ZERO",0);
    auto mask=b.addNewComponent<ConstantValue<4>>("READ_MASK",15);
    for(auto p:{"CLK","RST"}) {in(w,*this,pc,p);in(w,*this,request,p);}
    in(w,*this,pc,"PC_WRITE","WE");in(w,*this,pc,"NEXT_PC","D");
    for(auto p:{"ISSUE","TAKE"}) in(w,*this,request,p);
    auto value=pc->getOutputPin<32>("Q");
    w.connect(value,getOutputPin<32>("PC"));
    for(auto p:{"ADDR","TAG"}) w.connect(value,request->getInputPin<32>(p));
    w.connect(value,align->getInputPin<32>("ADDRESS"));
    w.connect(word->getOutputPin<2>("OUT"),align->getInputPin<2>("SIZE"));
    out(w,*this,align,"MISALIGNED","PC_MISALIGNED");
    w.connect(value,add->getInputPin<32>("A"));
    w.connect(four->getOutputPin<32>("OUT"),add->getInputPin<32>("B"));
    w.connect(zero,add->getInputPin("Cin"));
    out(w,*this,add,"Sum","PC_PLUS_4");
    w.connect(read->getOutputPin<3>("OUT"),request->getInputPin<3>("OP"));
    w.connect(data->getOutputPin<32>("OUT"),request->getInputPin<32>("WRITE_DATA"));
    w.connect(mask->getOutputPin<4>("OUT"),request->getInputPin<4>("WRITE_MASK"));
    for(auto p:{"DONE","FAULT"}) out(w,*this,request,p);
    out(w,*this,request,"READ_DATA","INSTRUCTION");
    rv32i::wireRequestBus(b,*this,request,"IMEM_");
    w.finish();
}

void RV32ISingleCycleDecodeStage::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto decode=b.add(circuit::families::RV32IDecodeControl,"CONTROL");
    registers_=b.add(circuit::families::RegisterFile32x32,"REGISTER_FILE",{}, {"register-state-view"});
    in(w,*this,decode,"INSTRUCTION");
    for(auto& [name,pin]:decode->getAllOutputPins()) w.connect(pin,getOutputPinDynamic(name));
    for(auto p:{"RS1_ADDR","RS2_ADDR","RD_ADDR"})
        w.connect(decode->getOutputPinDynamic(p),registers_->getInputPinDynamic(p));
    for(auto p:{"CLK","RST"}) in(w,*this,registers_,p);
    in(w,*this,registers_,"REGISTER_WRITE","REG_WRITE");
    in(w,*this,registers_,"WB_VALUE","WRITE_DATA");
    out(w,*this,registers_,"RS1_DATA","RS1_VALUE");
    out(w,*this,registers_,"RS2_DATA","RS2_VALUE");
    w.finish();
}
std::vector<std::vector<LogicValue>> RV32ISingleCycleDecodeStage::getRegisterStateAtTime(size_t t) const {
    auto view=std::dynamic_pointer_cast<RegisterStateView>(registers_);
    if(!view) throw std::logic_error("Decode register-state capability missing");
    return view->getRegisterStateAtTime(t);
}

void RV32ISingleCycleExecuteStage::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto alu=b.add(circuit::families::ALU32,"ALU");
    auto a=b.addNewComponent<Mux4to1_32bit>("OPERAND_A");
    auto c=b.addNewComponent<Mux4to1_32bit>("OPERAND_B");
    auto zero=b.addNewComponent<ConstantValue<32>>("ZERO32",0);
    auto low=w.constant("LOW",false);
    in(w,*this,a,"RS1_VALUE","IN0");in(w,*this,a,"PC","IN1");
    in(w,*this,c,"RS2_VALUE","IN0");in(w,*this,c,"IMM","IN1");
    for(auto mux:{a,c}) for(auto p:{"IN2","IN3"}) w.connect(zero->getOutputPin<32>("OUT"),mux->getInputPin<32>(p));
    in(w,*this,a,"ALU_A_SEL","SEL");in(w,*this,c,"ALU_B_SEL","SEL");
    w.connect(a->getOutputPin<32>("OUT"),alu->getInputPin<32>("A"));
    w.connect(c->getOutputPin<32>("OUT"),alu->getInputPin<32>("B"));
    in(w,*this,alu,"ALU_OP","OP");out(w,*this,alu,"OUT","ALU_RESULT");
    auto branch=b.addNewComponent<Mux8to1>("BRANCH_CONDITION");
    auto eq=alu->getOutputPin("EQ"),lt=alu->getOutputPin("LT_SIGNED"),ltu=alu->getOutputPin("LT_UNSIGNED");
    auto neq=w.invert("NOT_EQ",eq),ge=w.invert("NOT_LT",lt),geu=w.invert("NOT_LTU",ltu);
    std::vector<StructuralWiring::PinPtr> conditions{low,eq,neq,lt,ge,ltu,geu,low};
    for(size_t i=0;i<conditions.size();++i) w.connect(conditions[i],branch->getInputPin("IN"+std::to_string(i)));
    in(w,*this,branch,"BRANCH_TYPE","SEL");out(w,*this,branch,"OUT","BRANCH_TAKEN");
    // A branch needs this addition concurrently with the ALU comparison.
    // JAL/JALR instead reuse the main ALU; no RS1+IMM adder is duplicated.
    auto target_add=b.addNewComponent<Adder32>("BRANCH_TARGET_ADD");
    in(w,*this,target_add,"PC","A");in(w,*this,target_add,"IMM","B");
    w.connect(low,target_add->getInputPin("Cin"));
    auto mask=b.addNewComponent<Rewire>("JALR_MASK",std::vector<Rewire::WireSpec>{{"IN",32}},
        std::vector<Rewire::WireSpec>{{"OUT",32}},identity_mapping("IN",1,31,"OUT",1),Rewire::UnmappedBitValue::LOW);
    w.connect(alu->getOutputPin<32>("OUT"),mask->getInputPin<32>("IN"));
    auto target=b.addNewComponent<Mux4to1_32bit>("TARGET_SELECT");
    w.connect(target_add->getOutputPin<32>("Sum"),target->getInputPin<32>("IN0"));
    w.connect(alu->getOutputPin<32>("OUT"),target->getInputPin<32>("IN1"));
    w.connect(mask->getOutputPin<32>("OUT"),target->getInputPin<32>("IN2"));
    w.connect(zero->getOutputPin<32>("OUT"),target->getInputPin<32>("IN3"));
    in(w,*this,target,"JUMP_TYPE","SEL");out(w,*this,target,"OUT","CONTROL_TARGET");
    w.finish();
}

void RV32ISingleCycleMemoryStage::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto lanes=b.addNewComponent<RV32IMemoryLanes>("LANES");
    auto request=b.addNewComponent<RV32IMemoryRequest>("REQUEST");
    auto align=b.add(circuit::families::RV32IMemoryAlignmentUnit,"ALIGNMENT");
    auto op=b.addNewComponent<BitJoiner<3>>("OPERATION");
    auto zero=w.constant("ZERO",false);
    for(auto p:{"CLK","RST","ISSUE","TAKE"}) in(w,*this,request,p);
    for(auto p:{"ADDR","WRITE_DATA","SIZE","SIGN_EXTEND"}) in(w,*this,lanes,p);
    in(w,*this,request,"ADDR","TAG");
    in(w,*this,align,"ADDR","ADDRESS");in(w,*this,align,"SIZE");
    auto size_bits=b.addNewComponent<BitSplitter<2>>("SIZE_BITS");
    in(w,*this,size_bits,"SIZE","IN");
    auto invalid=w.both("INVALID_SIZE",size_bits->getOutputPin("OUT_0"),size_bits->getOutputPin("OUT_1"));
    auto bad=w.either("BAD_ADDRESS_OR_SIZE",invalid,align->getOutputPin("MISALIGNED"));
    w.connect(bad,getOutputPin("MISALIGNED"));
    in(w,*this,op,"MEM_WRITE","IN_0");
    w.connect(zero,op->getInputPin("IN_1"));w.connect(zero,op->getInputPin("IN_2"));
    w.connect(op->getOutputPin<3>("OUT"),request->getInputPin<3>("OP"));
    w.connect(lanes->getOutputPin<32>("WORD_ADDR"),request->getInputPin<32>("ADDR"));
    w.connect(lanes->getOutputPin<32>("WORD_WRITE_DATA"),request->getInputPin<32>("WRITE_DATA"));
    w.connect(lanes->getOutputPin<4>("WRITE_MASK"),request->getInputPin<4>("WRITE_MASK"));
    w.connect(request->getOutputPin<32>("READ_DATA"),lanes->getInputPin<32>("WORD_DATA"));
    out(w,*this,lanes,"READ_DATA");for(auto p:{"DONE","FAULT"}) out(w,*this,request,p);
    rv32i::wireRequestBus(b,*this,request,"DMEM_");
    w.finish();
}

void RV32ISingleCycleWritebackStage::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto select=b.addNewComponent<Mux4to1_32bit>("RESULT_SELECT");
    auto zero=b.addNewComponent<ConstantValue<32>>("ZERO32",0);
    w.connect(zero->getOutputPin<32>("OUT"),select->getInputPin<32>("IN0"));
    in(w,*this,select,"ALU_RESULT","IN1");in(w,*this,select,"READ_DATA","IN2");
    in(w,*this,select,"PC_PLUS_4","IN3");in(w,*this,select,"WRITEBACK_SEL","SEL");
    out(w,*this,select,"OUT","WB_VALUE");w.finish();
}

void RV32ISingleCycleNextPCStage::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto jump=b.addNewComponent<BitSplitter<2>>("JUMP_BITS");
    in(w,*this,jump,"JUMP_TYPE","IN");
    auto jumping=w.either("JUMPING",jump->getOutputPin("OUT_0"),jump->getOutputPin("OUT_1"));
    auto taken=w.either("TAKEN",jumping,getInputPin("BRANCH_TAKEN"));
    auto select=b.addNewComponent<Mux2to1_32bit>("NEXT_PC_SELECT");
    in(w,*this,select,"PC_PLUS_4","A");in(w,*this,select,"CONTROL_TARGET","B");
    w.connect(taken,select->getInputPin("SEL"));out(w,*this,select,"OUT","NEXT_PC");
    auto align=b.add(circuit::families::RV32IMemoryAlignmentUnit,"ALIGNMENT");
    auto word=b.addNewComponent<ConstantValue<2>>("WORD_SIZE",2);
    in(w,*this,align,"CONTROL_TARGET","ADDRESS");
    w.connect(word->getOutputPin<2>("OUT"),align->getInputPin<2>("SIZE"));
    auto misaligned=w.both("TAKEN_MISALIGNED",taken,align->getOutputPin("MISALIGNED"));
    w.connect(misaligned,getOutputPin("TARGET_MISALIGNED"));w.finish();
}
