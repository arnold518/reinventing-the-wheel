#include "../site-docs/rv32i/4.5/examples.hpp"
#include "tests/TestRegistry.hpp"
#include "tests/ComponentTestModel.hpp"
#include <iostream>
#include <map>
#include <stdexcept>

namespace {
using namespace baseline_example;
using U=std::map<std::string,uint32_t>;
uint32_t n(const circuit::test::LogicVector& bits) {
    uint32_t r=0;for(size_t i=0;i<bits.size();++i) {
        if(bits[i]!=LogicValue::LOW&&bits[i]!=LogicValue::HIGH) throw std::runtime_error("Nonbinary example input");
        if(bits[i]==LogicValue::HIGH) r|=uint32_t{1}<<i;
    }return r;
}
Packet packet(U& i,const std::string& prefix="IN_") {
    Packet p;p.valid=i[prefix+"VALID"];p.pc=i[prefix+"PC"];p.instruction=i[prefix+"INSTRUCTION"];p.fetch_status=i[prefix+"FETCH_STATUS"];
    p.rs1_value=i[prefix+"RS1_VALUE"];p.rs2_value=i[prefix+"RS2_VALUE"];p.immediate=i[prefix+"IMMEDIATE"];
    p.control=i[prefix+"CONTROL"];p.addresses=i[prefix+"REGISTER_ADDRESSES"];p.alu_result=i[prefix+"ALU_RESULT"];p.store_data=i[prefix+"STORE_DATA"];
    p.pc_plus_4=i[prefix+"PC_PLUS_4"];p.next_pc=i[prefix+"NEXT_PC"];p.execution_status=i[prefix+"EXECUTION_STATUS"];p.memory_data=i[prefix+"MEMORY_DATA"];p.memory_status=i[prefix+"MEMORY_STATUS"];return p;
}
U outputs(const Packet& p) {
    return {{"OUT_VALID",p.valid},{"OUT_PC",p.pc},{"OUT_INSTRUCTION",p.instruction},{"OUT_FETCH_STATUS",p.fetch_status},
        {"OUT_RS1_VALUE",p.rs1_value},{"OUT_RS2_VALUE",p.rs2_value},{"OUT_IMMEDIATE",p.immediate},{"OUT_CONTROL",p.control},{"OUT_REGISTER_ADDRESSES",p.addresses},
        {"OUT_ALU_RESULT",p.alu_result},{"OUT_STORE_DATA",p.store_data},{"OUT_PC_PLUS_4",p.pc_plus_4},{"OUT_NEXT_PC",p.next_pc},
        {"OUT_EXECUTION_STATUS",p.execution_status},{"OUT_MEMORY_DATA",p.memory_data},{"OUT_MEMORY_STATUS",p.memory_status}};
}
template<class Factory> void replay(const std::string& name,Factory factory) {
    auto test=createTestByName(name);const auto& spec=dynamic_cast<circuit::test::ComponentScenarioTest&>(*test).getSpec();size_t count=0;
    for(auto& scenario:spec.scenarios) {
        auto model=factory();U held;
        for(auto& action:scenario.actions) {
            for(auto& [pin,value]:action.inputs) held[pin]=n(value);
            const auto o=model(held);
            for(auto& [pin,value]:action.expected_outputs) if(o.at(pin)!=n(value))
                throw std::runtime_error(name+"/"+action.checkpoint_id+"/"+pin+": example="+std::to_string(o.at(pin))+" expected="+std::to_string(n(value)));
            ++count;
        }
    }std::cout<<name<<": exact documentation example passed "<<count<<" actions\n";
}
}
int main() {
    try {
        replay("RV32IBaselineFetchStageTest",[]{return [m=Fetch{}](U i) mutable {
            auto r=m.evaluate(i["CLK"],i["RST"],i["PC_WRITE"],i["PC_REDIRECT"],i["REDIRECT_PC"],i["FETCH_VALID"],i["IMEM_READ_DATA"],i["IMEM_FAULT"]);
            auto o=outputs(r.packet);o["FETCH_PC"]=r.fetch_pc;o["IMEM_WORD_ADDR"]=r.word_addr;o["IMEM_READ_EN"]=r.read_en;return o;
        };});
        replay("RV32IBaselineDecodeStageTest",[]{return [m=Decode{}](U i) mutable {
            auto r=m.evaluate(packet(i),i["CLK"],i["RST"],i["REGISTER_WRITE"],i["WB_RD"],i["WB_VALUE"]);
            auto o=outputs(r.packet);o["TERMINAL"]=r.terminal;o["CONTROL_FLOW"]=r.control_flow;return o;
        };});
        replay("RV32IBaselineExecuteStageTest",[]{return [](U i) {
            auto r=execute(packet(i));auto o=outputs(r.packet);o["TERMINAL"]=r.terminal;o["CONTROL_FLOW"]=r.control_flow;return o;
        };});
        replay("RV32IBaselineMemoryStageTest",[]{return [](U i) {
            auto r=memory(packet(i),i["MEMORY_ENABLE"],i["DMEM_READ_DATA"],i["DMEM_FAULT"]);auto o=outputs(r.packet);
            o["TERMINAL"]=r.terminal;o["DMEM_WORD_ADDR"]=r.word_addr;o["DMEM_WRITE_DATA"]=r.write_data;o["DMEM_WRITE_MASK"]=r.write_mask;o["DMEM_WE"]=r.we;o["DMEM_READ_EN"]=r.read_en;return o;
        };});
        replay("RV32IBaselineDependencyUnitTest",[]{return [](U i) {
            auto r=dependency(packet(i,"D_"),packet(i,"E_"),packet(i,"M_"),packet(i,"W_"));return U{{"WAIT_EX",r.wait_ex},{"WAIT_MEM",r.wait_mem},{"WAIT_WB",r.wait_wb},{"DATA_HAZARD",r.data_hazard}};
        };});
        replay("RV32IBaselineControlTest",[]{return [](U i) {
            auto r=control({bool(i["ENABLE"]),bool(i["RST"]),bool(i["HALTED"]),bool(i["TRAPPED"]),bool(i["DATA_HAZARD"]),bool(i["D_CONTROL_FLOW"]),bool(i["E_CONTROL_FLOW"]),
                bool(i["D_TERMINAL"]),bool(i["E_TERMINAL"]),bool(i["M_TERMINAL"]),bool(i["W_TERMINAL"])});
            return U{{"ACTIVE",r.active},{"FETCH_VALID",r.fetch_valid},{"PC_WRITE",r.pc_write},{"PC_REDIRECT",r.pc_redirect},{"IF_ID_WRITE",r.if_id_write},{"IF_ID_FLUSH",r.if_id_flush},
                {"ID_EX_WRITE",r.id_ex_write},{"ID_EX_FLUSH",r.id_ex_flush},{"EX_MEM_WRITE",r.ex_mem_write},{"EX_MEM_FLUSH",r.ex_mem_flush},{"MEM_WB_WRITE",r.mem_wb_write},{"MEM_WB_FLUSH",r.mem_wb_flush},
                {"MEMORY_ENABLE",r.memory_enable},{"COMMIT_ENABLE",r.commit_enable},{"DATA_STALL",r.data_stall},{"BRANCH_WAIT",r.branch_wait},{"PIPELINE_FLUSH",r.pipeline_flush}};
        };});
        replay("RV32IWritebackStageTest",[]{return [](U i) {
            auto r=writeback(packet(i),i["COMMIT_ENABLE"]);
            return U{{"HALT_EVENT",r.halt_event},{"TRAP_EVENT",r.trap_event},{"TRAP_CAUSE",r.cause},
                {"WB_VALID",r.valid},{"WB_REG_WRITE",r.reg_write},{"REGISTER_WRITE",r.register_write},{"WB_RD",r.rd},{"WB_VALUE",r.value},{"TERMINAL",r.terminal},{"RETIRE_TERMINAL",r.retire_terminal},{"COMMIT_VALID",r.commit_valid}};
        };});
        replay("RV32IPipelineExecutionStatusTest",[]{return [m=ExecutionStatus{}](U i) mutable {
            m.evaluate(i["CLK"],i["RST"],i["HALT_EVENT"],i["TRAP_EVENT"],i["EVENT_TRAP_CAUSE"]);
            return U{{"HALTED",m.halted},{"TRAPPED",m.trapped},{"TRAP_CAUSE",m.cause}};
        };});
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
