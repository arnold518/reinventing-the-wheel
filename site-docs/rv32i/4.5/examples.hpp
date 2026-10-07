// Standalone binary examples for section 4.4 (no simulator/event objects).
#pragma once
#include <array>
#include <cstdint>
#include "rv32i/RV32IDecoder.hpp"
#include "modules/rv32i/RV32IPipelineEncoding.hpp"
#include "modules/composite/ALU32.hpp"
namespace baseline_example {
using namespace rv32i::pipeline_encoding;
// --8<-- [start:packet]
struct Packet {
    bool valid=false;
    uint32_t pc=0, instruction=0, fetch_status=0;
    uint32_t rs1_value=0, rs2_value=0, immediate=0;
    uint32_t control=0, addresses=0;
    uint32_t alu_result=0, store_data=0, pc_plus_4=0, next_pc=0;
    uint32_t execution_status=0, memory_data=0, memory_status=0;
};
struct StageResult { Packet packet; bool terminal, control_flow; };
// --8<-- [end:packet]
inline bool early_terminal(const Packet& p) {
    auto c=unpackControl(p.control);
    return p.valid && ((p.fetch_status&3)||!c.legal||c.halt||c.trap||(p.execution_status&1));
}
// --8<-- [start:fetch]
struct Fetch {
    uint32_t pc=0;
    bool previous_clock=false;
    struct Outputs { Packet packet; uint32_t fetch_pc, word_addr; bool read_en; };
    Outputs evaluate(bool clk, bool reset, bool pc_write, bool redirect,
                     uint32_t redirect_pc, bool fetch_valid,
                     uint32_t instruction, bool fault) {
        if(reset) pc=0;
        else if(!previous_clock && clk && pc_write) pc=redirect?redirect_pc:pc+4;
        previous_clock=clk;
        Packet p;
        p.valid=fetch_valid; p.pc=pc; p.instruction=instruction;
        p.fetch_status=uint32_t(bool(pc&3)) | (uint32_t(fault)<<1);
        return {p,pc,pc>>2,fetch_valid};
    }
};
// --8<-- [end:fetch]
// --8<-- [start:decode]
struct Decode {
    std::array<uint32_t,32> registers{};
    bool previous_clock=false;
    StageResult evaluate(Packet p, bool clk, bool reset, bool register_write,
                         unsigned wb_rd, uint32_t wb_value) {
        if(reset) registers.fill(0);
        else if(!previous_clock && clk && register_write && (wb_rd&31))
            registers[wb_rd&31]=wb_value;
        previous_clock=clk;
        const auto d=rv32i::RV32IDecoder::decode(p.instruction);
        const auto c=rv32i::RV32IControl::fromDecoded(d);
        p.control=packControl(c); p.addresses=packAddresses(d.rs1,d.rs2,d.rd);
        p.immediate=uint32_t(d.immediate);
        p.rs1_value=d.rs1?registers[d.rs1]:0;
        p.rs2_value=d.rs2?registers[d.rs2]:0;
        const bool terminal=p.valid&&((p.fetch_status&3)||!c.legal||c.halt||c.trap);
        return {p,terminal,p.valid&&(unsigned(c.branch)||unsigned(c.jump))};
    }
};
// --8<-- [end:decode]
// --8<-- [start:alu]
inline uint32_t alu(unsigned op,uint32_t a,uint32_t b) {
    const unsigned sh=b&31;
    switch(op) {
        case ALU32Op::ADD:return a+b; case ALU32Op::SUB:return a-b;
        case ALU32Op::AND:return a&b; case ALU32Op::OR:return a|b; case ALU32Op::XOR:return a^b;
        case ALU32Op::SLL:return a<<sh; case ALU32Op::SRL:return a>>sh;
        case ALU32Op::SRA:return sh&&(a&0x80000000u)?(a>>sh)|(~uint32_t{0}<<(32-sh)):a>>sh;
        case ALU32Op::SLT:return int32_t(a)<int32_t(b); case ALU32Op::SLTU:return a<b;
        case ALU32Op::PASS_A:return a; case ALU32Op::PASS_B:return b; default:return 0;
    }
}
// --8<-- [end:alu]
// --8<-- [start:execute]
inline StageResult execute(Packet p) {
    const auto as=bits(p.control,27,3),bs=bits(p.control,29,3);
    const auto op=bits(p.control,22,31),branch=bits(p.control,13,7),jump=bits(p.control,16,3);
    const uint32_t a=as==0?p.rs1_value:as==1?p.pc:0;
    const uint32_t b=bs==0?p.rs2_value:bs==1?p.immediate:0;
    p.alu_result=alu(op,a,b); p.store_data=p.rs2_value; p.pc_plus_4=p.pc+4;
    const bool compare=op==ALU32Op::SUB||op==ALU32Op::SLT||op==ALU32Op::SLTU;
    const bool eq=compare&&a==b,lt=compare&&int32_t(a)<int32_t(b),ltu=compare&&a<b;
    const bool conditions[]{false,eq,!eq,lt,!lt,ltu,!ltu,false};
    const bool taken=jump||conditions[branch];
    const auto target=jump==0?p.pc+p.immediate:jump==1?p.alu_result:jump==2?p.alu_result&~1u:0;
    p.next_pc=taken?target:p.pc_plus_4;
    p.execution_status=taken&&bool(target&3);
    return {p,early_terminal(p),p.valid&&(branch||jump)};
}
// --8<-- [end:execute]
// --8<-- [start:memory]
struct MemoryResult {
    Packet packet; bool terminal;
    uint32_t word_addr, write_data; unsigned write_mask;
    bool read_en, we;
};
inline MemoryResult memory(Packet p,bool enable,uint32_t read_word,bool fault) {
    const bool early=early_terminal(p);
    const bool read=bits(p.control,2,1),write=bits(p.control,3,1),sign=bits(p.control,4,1);
    const auto size=bits(p.control,11,3),offset=p.alu_result&3;
    const bool eligible=p.valid&&!early&&(read||write);
    const bool bad=size==3||(size==1&&(offset&1))||(size==2&&offset);
    uint32_t value=read_word>>(8*offset);
    if(size==0) {value&=255; if(sign&&(value&128)) value|=0xffffff00u;}
    else if(size==1) {value&=65535; if(sign&&(value&32768)) value|=0xffff0000u;}
    else if(size==3) value=0;
    p.memory_data=value;
    p.memory_status=eligible?(uint32_t(bad)|(uint32_t(fault)<<1)):0;
    const unsigned mask=size==0?1u<<offset:size==1?(3u<<offset)&15:size==2?15:0;
    return {p,early||(eligible&&(bad||fault)),p.alu_result>>2,p.store_data<<(8*offset),mask,
            enable&&eligible,enable&&eligible&&write&&!bad};
}
// --8<-- [end:memory]
// --8<-- [start:dependency]
inline bool waits_for(const Packet& decode,const Packet& producer) {
    const auto destination=rd(producer.addresses);
    return decode.valid && producer.valid && bits(producer.control,1,1) && destination &&
        ((bits(decode.control,7,1)&&rs1(decode.addresses)==destination) ||
         (bits(decode.control,8,1)&&rs2(decode.addresses)==destination));
}
struct Dependency { bool wait_ex,wait_mem,wait_wb,data_hazard; };
inline Dependency dependency(const Packet& d,const Packet& e,const Packet& m,const Packet& w) {
    const bool ex=waits_for(d,e),mem=waits_for(d,m),wb=waits_for(d,w);
    return {ex,mem,wb,ex||mem||wb};
}
// --8<-- [end:dependency]
// --8<-- [start:control]
struct ControlInputs {
    bool enable,reset,halted,trapped,data_hazard,d_control_flow,e_control_flow;
    bool d_terminal,e_terminal,m_terminal,w_terminal;
};
struct ControlOutputs {
    bool active,fetch_valid,pc_write,pc_redirect;
    bool if_id_write,if_id_flush,id_ex_write,id_ex_flush;
    bool ex_mem_write,ex_mem_flush,mem_wb_write,mem_wb_flush;
    bool memory_enable,commit_enable,data_stall,branch_wait,pipeline_flush;
};
inline ControlOutputs control(const ControlInputs& i) {
    const bool active=i.enable&&!i.reset&&!i.halted&&!i.trapped;
    const bool late=i.m_terminal||i.w_terminal,older=i.e_terminal||late;
    const bool terminal=older||(i.d_terminal&&!i.e_control_flow);
    const bool stall=i.data_hazard&&!i.d_terminal&&!older;
    const bool branch=i.d_control_flow||i.e_control_flow;
    const bool fetch=active&&!terminal&&!stall&&!branch;
    const bool redirect=active&&i.e_control_flow&&!older;
    return {active,fetch,fetch||redirect,redirect,
        active&&(!stall||i.e_control_flow),terminal||branch,active,stall||older||i.e_control_flow,
        active,late,active,i.w_terminal,
        active&&!i.w_terminal,active,active&&stall,active&&branch,active&&(terminal||branch)};
}
// --8<-- [end:control]
// --8<-- [start:writeback]
inline unsigned trap_cause(const Packet& p) {
    const auto c=unpackControl(p.control);
    if(p.fetch_status&1) return 3;
    if(p.fetch_status&2) return 4;
    if(!c.legal) return 1;
    if(c.trap) return c.trap_cause==rv32i::RV32ITrapCause::EnvironmentCall?2:1;
    if(c.mem_read&&(p.memory_status&1)) return 5;
    if(c.mem_read&&(p.memory_status&2)) return 6;
    if(c.mem_write&&(p.memory_status&1)) return 7;
    if(c.mem_write&&(p.memory_status&2)) return 8;
    return p.execution_status&1?3:0;
}
struct WritebackOutputs {
    bool valid,reg_write,register_write,terminal,retire_terminal,commit_valid;
    unsigned rd; uint32_t value;
    bool halt_event,trap_event;
    unsigned cause;
};
inline WritebackOutputs writeback(const Packet& p,bool commit_enable) {
    const auto c=unpackControl(p.control);
    const auto trap=trap_cause(p);
    const bool terminal=p.valid&&(trap||c.halt),commit=commit_enable&&p.valid;
    const auto sel=bits(p.control,9,3);
    const uint32_t value=sel==1?p.alu_result:sel==2?p.memory_data:sel==3?p.pc_plus_4:0;
    return {p.valid,c.reg_write,commit&&!terminal&&c.reg_write,terminal,commit&&terminal,
        commit,rd(p.addresses),value,commit&&!trap&&c.halt,commit&&bool(trap),trap};
}
// --8<-- [end:writeback]
// --8<-- [start:execution-status]
struct ExecutionStatus {
    bool halted=false,trapped=false,previous_clock=false;
    unsigned cause=0;
    void evaluate(bool clk,bool reset,bool halt_event,bool trap_event,unsigned event_cause) {
        if(reset) {halted=trapped=false;cause=0;}
        else if(!previous_clock&&clk) {
            if(halt_event) halted=true;
            if(trap_event) {trapped=true;cause=event_cause&15;}
        }
        previous_clock=clk;
    }
};
// --8<-- [end:execution-status]
}
