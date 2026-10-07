// Binary, settled/edge examples. No simulator events or hidden structural model.
#pragma once
#include "../4.5/examples.hpp"
#include <algorithm>
#include <vector>
#include <stdexcept>
namespace advanced_example {
using baseline_example::Packet;
// --8<-- [start:forward]
struct Producer { bool valid,write,ready; unsigned rd; uint32_t value; };
inline unsigned forward_select(unsigned source,Producer ex,Producer wb) {
    source&=31;
    if(ex.valid&&ex.write&&(ex.rd&31)&&((ex.rd&31)==source)) return ex.ready?1:0;
    if(wb.valid&&wb.write&&(wb.rd&31)&&((wb.rd&31)==source)) return 2;
    return 0;
}
inline uint32_t forwarded(unsigned source,uint32_t captured,Producer ex,Producer wb) {
    switch(forward_select(source,ex,wb)) {
        case 1:return ex.value; case 2:return wb.value; default:return captured;
    }
}
inline baseline_example::StageResult execute(Packet p,Producer ex,Producer wb) {
    using namespace rv32i::pipeline_encoding;
    p.rs1_value=forwarded(rs1(p.addresses),p.rs1_value,ex,wb);
    p.rs2_value=forwarded(rs2(p.addresses),p.rs2_value,ex,wb);
    return baseline_example::execute(p); // The same ALU/target/status contract as 4.5.4.
}
// --8<-- [end:forward]
// --8<-- [start:hazard]
inline bool load_use(bool id_valid,unsigned rs1,unsigned rs2,bool uses1,bool uses2,
                     bool ex_valid,bool ex_load,unsigned rd) {
    rd&=31;rs1&=31;rs2&=31;
    return id_valid&&ex_valid&&ex_load&&rd&&((uses1&&rs1==rd)||(uses2&&rs2==rd));
}
// STALL_FETCH = STALL_IF_ID = BUBBLE_ID_EX = LOAD_USE_HAZARD = load_use(...).
// --8<-- [end:hazard]
// --8<-- [start:counter]
inline unsigned next_counter(unsigned state,bool taken,bool one_bit) {
    state&=3;
    if(one_bit) return taken?3:0;
    return unsigned(std::clamp(int(state)+(taken?1:-1),0,3));
}
// --8<-- [end:counter]
// --8<-- [start:predecode]
struct Predecode { bool conditional,direct_jump,backward; uint32_t target,pc_plus_4; };
inline Predecode predecode(uint32_t pc,uint32_t instruction) {
    const auto opcode=instruction&127,funct3=(instruction>>12)&7;
    const bool branch=opcode==0x63&&funct3!=2&&funct3!=3,jump=opcode==0x6f;
    uint32_t imm=0;
    if(branch) {
        imm=((instruction>>31)<<12)|(((instruction>>7)&1)<<11)|
            (((instruction>>25)&63)<<5)|(((instruction>>8)&15)<<1);
        if(imm&0x1000) imm|=0xffffe000u;
    } else if(jump) {
        imm=((instruction>>31)<<20)|(instruction&0xff000)|
            (((instruction>>20)&1)<<11)|(((instruction>>21)&1023)<<1);
        if(imm&0x100000) imm|=0xffe00000u;
    }
    return {branch,jump,bool(imm&0x80000000u),pc+imm,pc+4};
}
// --8<-- [end:predecode]
// --8<-- [start:predictor]
enum class Policy { NotTaken,Taken,BackwardTaken,OneBit,TwoBit };
struct Predictor {
    Policy policy;
    std::vector<unsigned> table;
    bool previous_clock=false;
    Predictor(Policy mode,size_t entries):policy(mode) {
        if(!entries||entries>256||(entries&(entries-1))) throw std::invalid_argument("table size");
        table.assign(entries,0);
    }
    unsigned counter(uint32_t pc) const {return table[(pc>>2)&(table.size()-1)];}
    struct Output {uint32_t next_pc;bool taken;unsigned counter;};
    Output lookup(uint32_t pc,Predecode d) const {
        const bool direction=policy==Policy::Taken ||
            (policy==Policy::BackwardTaken&&d.backward) ||
            ((policy==Policy::OneBit||policy==Policy::TwoBit)&&counter(pc)>=2);
        const bool taken=d.direct_jump||(d.conditional&&direction);
        return {taken?d.target:d.pc_plus_4,taken,counter(pc)};
    }
    void evaluate(bool clk,bool reset,bool update,uint32_t update_pc,bool taken) {
        if(reset) std::fill(table.begin(),table.end(),0);
        else if(!previous_clock&&clk&&update&&(policy==Policy::OneBit||policy==Policy::TwoBit)) {
            auto& state=table[(update_pc>>2)&(table.size()-1)];
            state=next_counter(state,taken,policy==Policy::OneBit);
        }
        previous_clock=clk;
    }
};
// --8<-- [end:predictor]
// --8<-- [start:recovery]
struct Recovery {bool mispredict,resolve,update,recovery,branch_miss;};
inline Recovery recover(bool valid,bool preterminal,bool advance,bool kill,bool branch,bool reset,
                        uint32_t predicted_pc,uint32_t fallthrough_pc,uint32_t target_pc,bool taken) {
    const bool eligible=valid&&!preterminal&&!reset;
    const bool different=taken ? predicted_pc!=target_pc : predicted_pc!=fallthrough_pc;
    const bool resolve=eligible&&advance&&!kill;
    return {eligible&&different,resolve,resolve&&branch,resolve&&different,resolve&&branch&&different};
}
// --8<-- [end:recovery]
// --8<-- [start:path]
struct PredictionPath {
    Predictor predictor;
    uint32_t if_id_prediction=0,id_ex_prediction=0;
    bool previous_clock=false;
    PredictionPath(Policy p,size_t entries):predictor(p,entries) {}
    void evaluate(bool clk,bool reset,bool if_id_write,bool id_ex_write,
                  uint32_t fetch_pc,uint32_t instruction,bool ex_valid,bool preterminal,
                  bool advance,bool kill,bool branch,uint32_t ex_pc,bool taken) {
        // Compute captures BEFORE changing the table or either packet register.
        const auto lookup=predictor.lookup(fetch_pc,predecode(fetch_pc,instruction)).next_pc;
        const auto old_if_id=if_id_prediction;
        if(reset) if_id_prediction=id_ex_prediction=0;
        else if(!previous_clock&&clk) {
            if(if_id_write) if_id_prediction=lookup;
            if(id_ex_write) id_ex_prediction=old_if_id;
        }
        predictor.evaluate(clk,reset,ex_valid&&!preterminal&&advance&&!kill&&branch,ex_pc,taken);
        previous_clock=clk;
    }
    // After settling: PREDICTED_NEXT_PC = predictor.lookup(...).next_pc;
    // EX_PREDICTED_NEXT_PC = id_ex_prediction;
    // actual_pc = EX_CONTROL_TAKEN ? EX_TARGET_PC : EX_FALLTHROUGH_PC;
    // FETCH_NEXT_PC = REDIRECT_APPLY ? actual_pc : PREDICTED_NEXT_PC;
    // Other outputs = recover(..., id_ex_prediction, EX_FALLTHROUGH_PC, EX_TARGET_PC, EX_CONTROL_TAKEN).
};
// --8<-- [end:path]
// --8<-- [start:coordinator]
struct ControlInputs {
    bool enable,halted,trapped,imem_ready,dmem_ready,load_use,terminal_pending;
    bool ex_preterminal,memory_terminal,wb_terminal,ex_redirect,memory_fault;
    bool mem_load,mem_store,retire_terminal;
};
struct ControlOutputs {
    bool active,imem_read,dmem_read,dmem_write,fetch_valid,pc_write,pc_redirect;
    bool if_id_write,if_id_flush,id_ex_write,id_ex_flush,ex_mem_write,ex_mem_flush;
    bool mem_wb_write,mem_wb_flush,commit,stall,load_use_stall,memory_stall,port_stall,flush;
    bool fetch_response_take,data_request_issue;
};
inline ControlOutputs control(ControlInputs i) {
    const bool active=i.enable&&!i.halted&&!i.trapped;
    const bool wait=active&&(i.mem_load||i.mem_store)&&!i.wb_terminal&&!i.dmem_ready;
    const bool conflict=active&&i.mem_load&&i.mem_store&&!i.wb_terminal&&i.dmem_ready;
    const bool terminal=i.terminal_pending||i.ex_preterminal||i.memory_terminal||i.wb_terminal;
    const bool retire=active&&!wait&&i.retire_terminal;
    const bool step=active&&!wait&&!conflict&&!retire;
    const bool kill=i.ex_preterminal||i.ex_redirect||i.memory_fault;
    const bool fetch=step&&!kill&&!i.load_use&&i.imem_ready&&!terminal;
    const bool redirect=step&&i.ex_redirect;
    const bool read=active&&i.mem_load&&!i.mem_store&&!i.wb_terminal;
    const bool write=active&&i.mem_store&&!i.wb_terminal;
    const bool if_flush=retire||(step&&kill);
    return {active,active&&!terminal,read,write,fetch,fetch||redirect,redirect,
        retire||(step&&!i.load_use),retire||(step&&kill),retire||step,retire||(step&&(kill||i.load_use)),
        retire||step,retire||(step&&i.memory_fault),retire||conflict||step,retire||conflict,
        active&&!wait,wait||conflict||i.load_use,step&&i.load_use,wait,conflict,step&&kill,
        fetch||((i.halted||i.trapped)||if_flush),read||write};
}
// --8<-- [end:coordinator]
}
