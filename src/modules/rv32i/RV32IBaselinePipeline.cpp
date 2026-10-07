#include "RV32IBaselinePipelineInternal.hpp"
#include "modules/memory/MemoryStructuralBuilder.hpp"
#include "modules/memory/RegisterFile32x32.hpp"
#include "modules/rv32i/RV32IDecodeControlUnit.hpp"
#include "modules/rv32i/RV32ISingleCycleStages.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include "modules/rv32i/RV32IMemorySystem.hpp"
#include "modules/composite/Adder32.hpp"
#include "modules/basic/Mux.hpp"
#include "modules/utility/Rewire.hpp"
#include <limits>

using circuit::memory_detail::StructuralWiring;
using namespace rv32i::baseline;
namespace {
using Ptr=std::shared_ptr<IOComponent>;
using Pin=StructuralWiring::PinPtr;
void packetPins(IOComponent* s, const std::vector<std::string>& fields, bool output) {
    const auto type=output?PinType::OUTPUT:PinType::INPUT;
    const std::string prefix=output?"OUT_":"IN_";
    s->addPin(prefix+"VALID",type);
    for(auto& f:fields) s->addPin<32>(prefix+f,type);
}
void fetchPins(IOComponent* s) {
    for(auto p:{"CLK","RST","PC_WRITE","PC_REDIRECT","FETCH_VALID"}) s->addPin(p,PinType::INPUT);
    s->addPin<32>("REDIRECT_PC",PinType::INPUT);
    rv32i::directMemoryPins(s,"IMEM_",true);
    s->addPin<32>("FETCH_PC",PinType::OUTPUT);
    packetPins(s,fetchFields,true);
}
void decodePins(IOComponent* s) {
    packetPins(s,fetchFields,false);packetPins(s,decodeFields,true);
    for(auto p:{"CLK","RST","REGISTER_WRITE"}) s->addPin(p,PinType::INPUT);
    s->addPin<5>("WB_RD",PinType::INPUT);s->addPin<32>("WB_VALUE",PinType::INPUT);
    for(auto p:{"TERMINAL","CONTROL_FLOW"}) s->addPin(p,PinType::OUTPUT);
}
void executePins(IOComponent* s) {
    packetPins(s,decodeFields,false);packetPins(s,executeFields,true);
    for(auto p:{"TERMINAL","CONTROL_FLOW"}) s->addPin(p,PinType::OUTPUT);
}
void memoryPins(IOComponent* s) {
    packetPins(s,executeFields,false);packetPins(s,memoryFields,true);
    s->addPin("MEMORY_ENABLE",PinType::INPUT);
    rv32i::directMemoryPins(s,"DMEM_");
    s->addPin("TERMINAL",PinType::OUTPUT);
}
void dependencyPins(IOComponent* s) {
    for(const std::string p:{"D","E","M","W"}) {
        s->addPin(p+"_VALID",PinType::INPUT);
        s->addPin<32>(p+"_CONTROL",PinType::INPUT);
        s->addPin<32>(p+"_REGISTER_ADDRESSES",PinType::INPUT);
    }
    for(auto p:{"DATA_HAZARD","WAIT_EX","WAIT_MEM","WAIT_WB"}) s->addPin(p,PinType::OUTPUT);
}
void controlPins(IOComponent* s) {
    for(auto p:{"ENABLE","RST","HALTED","TRAPPED","DATA_HAZARD","D_CONTROL_FLOW","E_CONTROL_FLOW",
            "D_TERMINAL","E_TERMINAL","M_TERMINAL","W_TERMINAL"}) s->addPin(p,PinType::INPUT);
    for(auto p:{"ACTIVE","FETCH_VALID","PC_WRITE","PC_REDIRECT","IF_ID_WRITE","IF_ID_FLUSH",
            "ID_EX_WRITE","ID_EX_FLUSH","EX_MEM_WRITE","EX_MEM_FLUSH","MEM_WB_WRITE","MEM_WB_FLUSH",
            "MEMORY_ENABLE","COMMIT_ENABLE","DATA_STALL","BRANCH_WAIT","PIPELINE_FLUSH"}) s->addPin(p,PinType::OUTPUT);
}
void input(StructuralWiring& w,IOComponent& s,Ptr c,const std::string& p,const std::string& q="") {
    w.connect(s.getInputPinDynamic(p),c->getInputPinDynamic(q.empty()?p:q));
}
void output(StructuralWiring& w,IOComponent& s,Ptr c,const std::string& p,const std::string& q="") {
    w.connect(c->getOutputPinDynamic(p),s.getOutputPinDynamic(q.empty()?p:q));
}
struct Field {const char* name;size_t width,offset;};
const std::vector<Field> controls{{"LEGAL",1,0},{"REG_WRITE",1,1},{"MEM_READ",1,2},{"MEM_WRITE",1,3},
    {"LOAD_SIGN_EXTEND",1,4},{"HALT_REQUEST",1,5},{"TRAP_REQUEST",1,6},{"USES_RS1",1,7},{"USES_RS2",1,8},
    {"WRITEBACK_SEL",2,9},{"MEM_SIZE",2,11},{"BRANCH_TYPE",3,13},{"JUMP_TYPE",2,16},
    {"DECODE_TRAP_CAUSE",4,18},{"ALU_OP",5,22},{"ALU_A_SEL",2,27},{"ALU_B_SEL",2,29}};
Ptr codec(ComponentBuilder& b,const std::string& name,const std::string& bus,
        const std::vector<Field>& fields,bool pack) {
    std::vector<Rewire::WireSpec> a,c{{bus,32}};
    std::vector<Rewire::BitMap> maps;
    for(auto& f:fields) {
        a.emplace_back(f.name,f.width);
        auto m=pack?identity_mapping(f.name,0,f.width,bus,f.offset):identity_mapping(bus,f.offset,f.width,f.name);
        maps.insert(maps.end(),m.begin(),m.end());
    }
    return b.addNewComponent<Rewire>(name,pack?a:c,pack?c:a,maps,Rewire::UnmappedBitValue::LOW);
}
// Real boundary outputs need a driver, not an input-to-output alias.
void pass(ComponentBuilder& b,StructuralWiring& w,IOComponent& s,const std::vector<std::string>& fields) {
    std::vector<Rewire::WireSpec> ins,outs;std::vector<Rewire::BitMap> maps;
    for(auto& f:fields) {
        const size_t width=f=="VALID"?1:32;
        ins.emplace_back("IN_"+f,width);outs.emplace_back("OUT_"+f,width);
        auto m=identity_mapping("IN_"+f,0,width,"OUT_"+f);maps.insert(maps.end(),m.begin(),m.end());
    }
    auto c=b.addNewComponent<Rewire>("PACKET_PASS",ins,outs,maps);
    for(auto& f:fields) {input(w,s,c,"IN_"+f);output(w,s,c,"OUT_"+f);}
}
Pin nonzero(ComponentBuilder& b,StructuralWiring& w,const std::string& name,Pin p) {
    std::vector<Rewire::WireSpec> outs;std::vector<Rewire::BitMap> maps;
    for(size_t i=0;i<p->getWidth();++i) {outs.emplace_back("B"+std::to_string(i),1);maps.emplace_back("IN",i,outs.back().name,0);}
    auto r=b.addNewComponent<Rewire>(name+"_BITS",std::vector<Rewire::WireSpec>{{"IN",p->getWidth()}},outs,maps);
    w.connect(p,r->getInputPinDynamic("IN"));std::vector<Pin> pins;
    for(auto& o:outs) pins.push_back(r->getOutputPin(o.name));
    return w.reduce(name,pins,false);
}
// Only the defined status bits participate; reserved packet bits are ignored.
Pin earlyTerminal(ComponentBuilder& b,StructuralWiring& w,Pin valid,Ptr ctrl,Pin fetch,Pin target={}) {
    auto status=codec(b,"FETCH_STATUS","STATUS",{{"PC_BAD",1,0},{"FETCH_BAD",1,1}},false);
    w.connect(fetch,status->getInputPin<32>("STATUS"));
    std::vector<Pin> reasons{status->getOutputPin("PC_BAD"),status->getOutputPin("FETCH_BAD"),
        w.invert("ILLEGAL",ctrl->getOutputPin("LEGAL")),ctrl->getOutputPin("HALT_REQUEST"),ctrl->getOutputPin("TRAP_REQUEST")};
    if(target) reasons.push_back(target);
    return w.both("VALID_TERMINAL",valid,w.reduce("TERMINAL_REASONS",reasons,false));
}
Pin controlFlow(ComponentBuilder& b,StructuralWiring& w,Pin valid,Ptr ctrl) {
    // Component insertion order is serialized by the visualizer. Do not rely
    // on C++ function-argument evaluation order (GCC and Clang differ here).
    const auto branch=nonzero(b,w,"BRANCH",ctrl->getOutputPin<3>("BRANCH_TYPE"));
    const auto jump=nonzero(b,w,"JUMP",ctrl->getOutputPin<2>("JUMP_TYPE"));
    return w.both("VALID_CONTROL_FLOW",valid,w.either("IS_CONTROL_FLOW",branch,jump));
}

// A structural grouping, not a new selectable stage or a behavioral shortcut.
// The enclosing stage's equivalence tests exercise these exact gates.
class PacketStatus : public IOComponent {
public:
    explicit PacketStatus(std::string name):IOComponent(std::move(name),[](IOComponent* s) {
        s->addPin("VALID",PinType::INPUT);
        for(auto p:{"CONTROL","FETCH_STATUS","EXECUTION_STATUS"}) s->addPin<32>(p,PinType::INPUT);
        for(auto p:{"TERMINAL","CONTROL_FLOW"}) s->addPin(p,PinType::OUTPUT);
    }) {}
    const char* getTypeName() const override {return "RV32IBaselinePacketStatus";}
    void buildInternals(ComponentBuilder& b) override {
        StructuralWiring w(b);
        auto ctrl=codec(b,"CONTROL_FIELDS","CONTROL",{{"LEGAL",1,0},{"HALT_REQUEST",1,5},{"TRAP_REQUEST",1,6},
            {"BRANCH_TYPE",3,13},{"JUMP_TYPE",2,16}},false);
        auto exec=codec(b,"EXECUTION_STATUS","STATUS",{{"TARGET_BAD",1,0}},false);
        input(w,*this,ctrl,"CONTROL");input(w,*this,exec,"EXECUTION_STATUS","STATUS");
        w.connect(earlyTerminal(b,w,getInputPin("VALID"),ctrl,getInputPin<32>("FETCH_STATUS"),exec->getOutputPin("TARGET_BAD")),getOutputPin("TERMINAL"));
        w.connect(controlFlow(b,w,getInputPin("VALID"),ctrl),getOutputPin("CONTROL_FLOW"));w.finish();
    }
};
Ptr packetStatus(ComponentBuilder& b,StructuralWiring& w,IOComponent& s,Pin control,Pin execution={}) {
    auto status=b.addNewComponent<PacketStatus>("PACKET_STATUS");
    input(w,s,status,"IN_VALID","VALID");input(w,s,status,"IN_FETCH_STATUS","FETCH_STATUS");
    w.connect(control,status->getInputPin<32>("CONTROL"));
    if(!execution) execution=b.addNewComponent<ConstantValue<32>>("NO_EXECUTION_FAULT",0)->getOutputPin<32>("OUT");
    w.connect(execution,status->getInputPin<32>("EXECUTION_STATUS"));return status;
}
}

#define DEFINE_BLOCK(Name,Id,Pins) \
namespace circuit::families {const ComponentFamily RV32IBaseline##Name{ \
    "rv32i.baseline." Id,"RV32IBaseline" #Name,Pins, \
    [](const std::string& n,const std::shared_ptr<BuildContext>& c) {return Component::createWithContext<::RV32IBaseline##Name>(c,n);}, \
    [](const std::string& n,const std::shared_ptr<BuildContext>& c) {return rv32i::baseline::direct(Block::Name,n,c);}};} \
RV32IBaseline##Name::RV32IBaseline##Name(std::string n):IOComponent(std::move(n),Pins) {}
DEFINE_BLOCK(FetchStage,"fetch",fetchPins)
DEFINE_BLOCK(DecodeStage,"decode",decodePins)
DEFINE_BLOCK(ExecuteStage,"execute",executePins)
DEFINE_BLOCK(MemoryStage,"memory",memoryPins)
DEFINE_BLOCK(DependencyUnit,"dependency",dependencyPins)
DEFINE_BLOCK(Control,"control",controlPins)
#undef DEFINE_BLOCK
namespace rv32i::baseline {
const circuit::ComponentFamily& family(Block block) {
    switch(block) {
#define CASE(Name) case Block::Name:return circuit::families::RV32IBaseline##Name;
        CASE(FetchStage) CASE(DecodeStage) CASE(ExecuteStage) CASE(MemoryStage) CASE(DependencyUnit) CASE(Control)
#undef CASE
    }
    throw std::logic_error("Invalid baseline pipeline block");
}
}

void RV32IBaselineFetchStage::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto fetch=b.add(circuit::families::RV32ISingleCycleFetchStage,"FETCH_DATAPATH");
    auto next=b.addNewComponent<Mux2to1_32bit>("NEXT_PC_SELECT");
    auto pack=codec(b,"FETCH_STATUS_PACK","STATUS",{{"PC_BAD",1,0},{"FETCH_BAD",1,1}},true);
    for(auto p:{"CLK","RST","PC_WRITE","IMEM_READ_DATA","IMEM_FAULT"}) input(w,*this,fetch,p);
    input(w,*this,fetch,"FETCH_VALID","ISSUE");input(w,*this,next,"PC_REDIRECT","SEL");
    input(w,*this,next,"REDIRECT_PC","B");
    w.connect(fetch->getOutputPin<32>("PC_PLUS_4"),next->getInputPin<32>("A"));
    w.connect(next->getOutputPin<32>("OUT"),fetch->getInputPin<32>("NEXT_PC"));
    output(w,*this,fetch,"PC","FETCH_PC");output(w,*this,fetch,"PC","OUT_PC");
    output(w,*this,fetch,"INSTRUCTION","OUT_INSTRUCTION");
    for(auto p:{"IMEM_WORD_ADDR","IMEM_READ_EN"}) output(w,*this,fetch,p);
    output(w,*this,fetch,"IMEM_READ_EN","OUT_VALID");
    w.connect(fetch->getOutputPin("PC_MISALIGNED"),pack->getInputPin("PC_BAD"));
    w.connect(fetch->getOutputPin("FAULT"),pack->getInputPin("FETCH_BAD"));
    output(w,*this,pack,"STATUS","OUT_FETCH_STATUS");w.finish();
}

void RV32IBaselineDecodeStage::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto ctrl=b.add(circuit::families::RV32IDecodeControl,"CONTROL");
    registers_=b.add(circuit::families::RegisterFile32x32,"REGISTER_FILE",{}, {"register-state-view"});
    auto pack=codec(b,"CONTROL_PACK","CONTROL",controls,true);
    auto addr=codec(b,"REGISTER_ADDRESSES","ADDRESSES",{{"RS1",5,0},{"RS2",5,5},{"RD",5,10}},true);
    pass(b,w,*this,{"VALID","PC","INSTRUCTION","FETCH_STATUS"});
    input(w,*this,ctrl,"IN_INSTRUCTION","INSTRUCTION");
    for(auto& f:controls) w.connect(ctrl->getOutputPinDynamic(f.name),pack->getInputPinDynamic(f.name));
    for(const std::string p:{"RS1","RS2","RD"}) w.connect(ctrl->getOutputPin<5>(p+"_ADDR"),addr->getInputPin<5>(p));
    for(const std::string p:{"RS1_ADDR","RS2_ADDR"}) w.connect(ctrl->getOutputPin<5>(p),registers_->getInputPin<5>(p));
    for(auto p:{"CLK","RST"}) input(w,*this,registers_,p);
    input(w,*this,registers_,"WB_RD","RD_ADDR");input(w,*this,registers_,"WB_VALUE","WRITE_DATA");
    input(w,*this,registers_,"REGISTER_WRITE","REG_WRITE");
    output(w,*this,registers_,"RS1_DATA","OUT_RS1_VALUE");output(w,*this,registers_,"RS2_DATA","OUT_RS2_VALUE");
    output(w,*this,ctrl,"IMM","OUT_IMMEDIATE");output(w,*this,pack,"CONTROL","OUT_CONTROL");
    output(w,*this,addr,"ADDRESSES","OUT_REGISTER_ADDRESSES");
    auto status=packetStatus(b,w,*this,pack->getOutputPin<32>("CONTROL"));
    for(auto p:{"TERMINAL","CONTROL_FLOW"}) output(w,*this,status,p);
    w.finish();
}
std::vector<std::vector<LogicValue>> RV32IBaselineDecodeStage::getRegisterStateAtTime(size_t t) const {
    return std::dynamic_pointer_cast<RegisterStateView>(registers_)->getRegisterStateAtTime(t);
}

void RV32IBaselineExecuteStage::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto fields=codec(b,"CONTROL_FIELDS","CONTROL",controls,false);
    auto alu=b.add(circuit::families::RV32ISingleCycleExecuteStage,"DATAPATH");
    auto next=b.add(circuit::families::RV32ISingleCycleNextPCStage,"NEXT_PC");
    auto pc4=b.addNewComponent<Adder32>("PC_PLUS_4_ADD");
    auto four=b.addNewComponent<ConstantValue<32>>("FOUR",4);
    auto status=codec(b,"EXECUTION_STATUS_PACK","STATUS",{{"TARGET_BAD",1,0}},true);
    pass(b,w,*this,{"VALID","PC","INSTRUCTION","CONTROL","REGISTER_ADDRESSES","FETCH_STATUS"});
    input(w,*this,fields,"IN_CONTROL","CONTROL");
    for(const std::string p:{"PC","RS1_VALUE","RS2_VALUE"}) input(w,*this,alu,"IN_"+p,p);
    input(w,*this,alu,"IN_IMMEDIATE","IMM");
    for(auto p:{"ALU_OP","ALU_A_SEL","ALU_B_SEL","BRANCH_TYPE","JUMP_TYPE"})
        w.connect(fields->getOutputPinDynamic(p),alu->getInputPinDynamic(p));
    input(w,*this,pc4,"IN_PC","A");w.connect(four->getOutputPin<32>("OUT"),pc4->getInputPin<32>("B"));
    w.connect(w.constant("LOW",false),pc4->getInputPin("Cin"));
    w.connect(pc4->getOutputPin<32>("Sum"),next->getInputPin<32>("PC_PLUS_4"));
    for(auto p:{"CONTROL_TARGET","BRANCH_TAKEN"}) w.connect(alu->getOutputPinDynamic(p),next->getInputPinDynamic(p));
    w.connect(fields->getOutputPin<2>("JUMP_TYPE"),next->getInputPin<2>("JUMP_TYPE"));
    output(w,*this,pc4,"Sum","OUT_PC_PLUS_4");output(w,*this,alu,"ALU_RESULT","OUT_ALU_RESULT");
    output(w,*this,next,"NEXT_PC","OUT_NEXT_PC");
    // Store data is the captured pre-instruction RS2 value, never an ALU result.
    auto store=b.addNewComponent<Rewire>("STORE_DATA",std::vector<Rewire::WireSpec>{{"IN",32}},
        std::vector<Rewire::WireSpec>{{"OUT",32}},identity_mapping("IN",0,32,"OUT"));
    input(w,*this,store,"IN_RS2_VALUE","IN");output(w,*this,store,"OUT","OUT_STORE_DATA");
    w.connect(next->getOutputPin("TARGET_MISALIGNED"),status->getInputPin("TARGET_BAD"));
    output(w,*this,status,"STATUS","OUT_EXECUTION_STATUS");
    auto packet_status=packetStatus(b,w,*this,getInputPin<32>("IN_CONTROL"),status->getOutputPin<32>("STATUS"));
    for(auto p:{"TERMINAL","CONTROL_FLOW"}) output(w,*this,packet_status,p);
    w.finish();
}

void RV32IBaselineMemoryStage::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);
    auto fields=codec(b,"CONTROL_FIELDS","CONTROL",controls,false);
    auto status=codec(b,"MEMORY_STATUS_PACK","STATUS",{{"MISALIGNED",1,0},{"FAULT",1,1}},true);
    auto memory=b.add(circuit::families::RV32ISingleCycleMemoryStage,"ACCESS");
    std::vector<std::string> carry=executeFields;carry.push_back("VALID");pass(b,w,*this,carry);
    input(w,*this,fields,"IN_CONTROL","CONTROL");
    auto packet_status=packetStatus(b,w,*this,getInputPin<32>("IN_CONTROL"),getInputPin<32>("IN_EXECUTION_STATUS"));
    const auto terminal=packet_status->getOutputPin("TERMINAL");
    auto op=w.either("MEMORY_OPERATION",fields->getOutputPin("MEM_READ"),fields->getOutputPin("MEM_WRITE"));
    auto eligible=w.reduce("ELIGIBLE",{getInputPin("IN_VALID"),w.invert("NOT_TERMINAL",terminal),op},true);
    auto issue=w.both("ISSUE",eligible,getInputPin("MEMORY_ENABLE"));
    w.connect(issue,memory->getInputPin("ISSUE"));
    // ISSUE excludes older faults. The endpoint rejects invalid stores;
    // response status must not change the operation being validated.
    w.connect(w.constant("STORE_AUTHORIZED",true),memory->getInputPin("COMMIT"));
    w.connect(fields->getOutputPin("MEM_WRITE"),memory->getInputPin("MEM_WRITE"));
    w.connect(fields->getOutputPin("LOAD_SIGN_EXTEND"),memory->getInputPin("SIGN_EXTEND"));
    w.connect(fields->getOutputPin<2>("MEM_SIZE"),memory->getInputPin<2>("SIZE"));
    input(w,*this,memory,"IN_ALU_RESULT","ADDR");input(w,*this,memory,"IN_STORE_DATA","WRITE_DATA");
    for(auto p:{"DMEM_READ_DATA","DMEM_FAULT"}) input(w,*this,memory,p);
    for(auto p:{"DMEM_WORD_ADDR","DMEM_READ_EN","DMEM_WRITE_DATA","DMEM_WRITE_MASK","DMEM_WE"}) output(w,*this,memory,p);
    output(w,*this,memory,"READ_DATA","OUT_MEMORY_DATA");
    auto bad=w.both("ACTIVE_MISALIGNED",eligible,memory->getOutputPin("MISALIGNED"));
    auto fault=w.both("ACTIVE_FAULT",eligible,memory->getOutputPin("FAULT"));
    w.connect(bad,status->getInputPin("MISALIGNED"));w.connect(fault,status->getInputPin("FAULT"));
    output(w,*this,status,"STATUS","OUT_MEMORY_STATUS");
    w.connect(w.reduce("TERMINAL",{terminal,bad,fault},false),getOutputPin("TERMINAL"));w.finish();
}

void RV32IBaselineDependencyUnit::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);std::vector<StructuralWiring::PinPtr> hazards;
    // Reuse the already-tested two-source comparator slice. Its historical
    // EX_MEM_READ pin is the producer enable: here REG_WRITE, not just loads.
    auto d=codec(b,"DECODE_FIELDS","CONTROL",{{"USES_RS1",1,7},{"USES_RS2",1,8}},false);
    auto a=codec(b,"SOURCE_REGISTERS","ADDRESSES",{{"RS1",5,0},{"RS2",5,5}},false);
    input(w,*this,d,"D_CONTROL","CONTROL");input(w,*this,a,"D_REGISTER_ADDRESSES","ADDRESSES");
    for(const std::string stage:{"E","M","W"}) {
        const std::string name=stage=="E"?"WAIT_EX":stage=="M"?"WAIT_MEM":"WAIT_WB";
        auto c=codec(b,stage+"_WRITE_CONTROL","CONTROL",{{"WRITE",1,1}},false);
        auto rd=codec(b,stage+"_DESTINATION","ADDRESSES",{{"RD",5,10}},false);
        auto h=b.add(circuit::families::RV32IHazardDetectionUnit,name);
        input(w,*this,c,stage+"_CONTROL","CONTROL");input(w,*this,rd,stage+"_REGISTER_ADDRESSES","ADDRESSES");
        input(w,*this,h,"D_VALID","ID_VALID");input(w,*this,h,stage+"_VALID","EX_VALID");
        w.connect(c->getOutputPin("WRITE"),h->getInputPin("EX_MEM_READ"));
        w.connect(rd->getOutputPin<5>("RD"),h->getInputPin<5>("EX_RD"));
        for(const std::string n:{"RS1","RS2"}) {
            w.connect(a->getOutputPin<5>(n),h->getInputPin<5>("ID_"+n));
            w.connect(d->getOutputPin("USES_"+n),h->getInputPin("ID_USES_"+n));
        }
        hazards.push_back(h->getOutputPin("LOAD_USE_HAZARD"));output(w,*this,h,"LOAD_USE_HAZARD",name);
    }
    w.connect(w.reduce("ANY_DEPENDENCY",hazards,false),getOutputPin("DATA_HAZARD"));w.finish();
}

void RV32IBaselineControl::buildInternals(ComponentBuilder& b) {
    StructuralWiring w(b);auto p=[&](const char* n){return getInputPin(n);};
    auto out=[&](const char* n,StructuralWiring::PinPtr v){w.connect(v,getOutputPin(n));};
    auto active=w.reduce("ACTIVE",{p("ENABLE"),w.invert("NOT_RESET",p("RST")),
        w.invert("NOT_HALTED",p("HALTED")),w.invert("NOT_TRAPPED",p("TRAPPED"))},true);
    auto late=w.either("LATE_TERMINAL",p("M_TERMINAL"),p("W_TERMINAL"));
    auto older=w.either("OLDER_TERMINAL",p("E_TERMINAL"),late);
    auto decoded=w.both("LIVE_D_TERMINAL",p("D_TERMINAL"),w.invert("NOT_E_CONTROL_FLOW",p("E_CONTROL_FLOW")));
    auto terminal=w.either("ANY_TERMINAL",older,decoded);
    auto stall=w.reduce("DEPENDENCY_WAIT",{p("DATA_HAZARD"),w.invert("NOT_D_TERMINAL",p("D_TERMINAL")),w.invert("NOT_OLDER_TERMINAL",older)},true);
    auto branch=w.either("CONTROL_FLOW_WAIT",p("D_CONTROL_FLOW"),p("E_CONTROL_FLOW"));
    auto fetch=w.reduce("FETCH_ACCEPT",{active,w.invert("NO_TERMINAL",terminal),w.invert("NO_DEPENDENCY",stall),w.invert("NO_BRANCH",branch)},true);
    auto redirect=w.reduce("RESOLVE",{active,p("E_CONTROL_FLOW"),w.invert("NO_OLDER_TERMINAL",older)},true);
    out("ACTIVE",active);out("COMMIT_ENABLE",active);out("FETCH_VALID",fetch);
    out("PC_WRITE",w.either("ADVANCE_PC",fetch,redirect));out("PC_REDIRECT",redirect);
    out("IF_ID_WRITE",w.both("DECODE_WRITE",active,w.either("DECODE_RELEASE",w.invert("NO_STALL",stall),p("E_CONTROL_FLOW"))));
    out("IF_ID_FLUSH",w.either("DECODE_BUBBLE",terminal,branch));
    for(auto n:{"ID_EX_WRITE","EX_MEM_WRITE","MEM_WB_WRITE"}) out(n,active);
    out("ID_EX_FLUSH",w.reduce("EXECUTE_BUBBLE",{stall,older,p("E_CONTROL_FLOW")},false));
    out("EX_MEM_FLUSH",late);
    auto wb_flush=b.addNewComponent<Rewire>("WRITEBACK_FLUSH",std::vector<Rewire::WireSpec>{{"IN",1}},
        std::vector<Rewire::WireSpec>{{"OUT",1}},identity_mapping("IN",0,1,"OUT"));
    w.connect(p("W_TERMINAL"),wb_flush->getInputPin("IN"));out("MEM_WB_FLUSH",wb_flush->getOutputPin("OUT"));
    out("MEMORY_ENABLE",w.both("MEMORY_ALLOWED",active,w.invert("NO_W_TERMINAL",p("W_TERMINAL"))));
    out("DATA_STALL",w.both("ACTIVE_STALL",active,stall));
    out("BRANCH_WAIT",w.both("ACTIVE_BRANCH_WAIT",active,branch));
    out("PIPELINE_FLUSH",w.both("ACTIVE_FLUSH",active,w.either("FLUSH_REASON",terminal,branch)));w.finish();
}
