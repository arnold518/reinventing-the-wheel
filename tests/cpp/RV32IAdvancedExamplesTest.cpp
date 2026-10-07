#include "../../site-docs/rv32i/4.6/examples.hpp"
#include "tests/TestRegistry.hpp"
#include "tests/ComponentTestModel.hpp"
#include "rv32i/RV32IPrediction.hpp"
#include <iostream>
#include <map>
#include <functional>
namespace {
using namespace advanced_example;
using U=std::map<std::string,uint32_t>;
uint32_t number(const circuit::test::LogicVector& bits) {
    uint32_t n=0;
    for(size_t i=0;i<bits.size();++i) {
        if(bits[i]!=LogicValue::LOW&&bits[i]!=LogicValue::HIGH) throw std::runtime_error("nonbinary example input");
        n|=uint32_t(bits[i]==LogicValue::HIGH)<<i;
    }return n;
}
U recoveryOutputs(Recovery r) {return {{"MISPREDICT",r.mispredict},{"RESOLVE",r.resolve},{"UPDATE",r.update},{"RECOVERY",r.recovery},{"BRANCH_MISS",r.branch_miss}};}
Policy policy(const circuit::ParameterMap& p) {return Policy(unsigned(rv32i::predictionConfig(p).policy));}
template<class Factory> void replay(const std::string& name,Factory factory) {
    auto test=createTestByName(name);
    const auto& spec=dynamic_cast<circuit::test::ComponentScenarioTest&>(*test).getSpec();size_t count=0;
    for(auto& scenario:spec.scenarios) {
        auto model=factory(scenario.parameters.empty()?spec.parameters:scenario.parameters);U held;
        for(auto& action:scenario.actions) {
            for(auto& [pin,value]:action.inputs) held[pin]=number(value);
            const auto out=model(held);
            for(auto& [pin,value]:action.expected_outputs) if(out.at(pin)!=number(value))
                throw std::runtime_error(name+"/"+scenario.id+"/"+action.checkpoint_id+"/"+pin+": example="+std::to_string(out.at(pin))+" expected="+std::to_string(number(value)));
            ++count;
        }
    }std::cout<<name<<": "<<count<<" documentation actions passed\n";
}
Producer producer(U i,const std::string& p) {
    return {bool(i[p+"_VALID"]),bool(i[p+"_REG_WRITE"]),p=="EX_MEM"?bool(i[p+"_RESULT_READY"]):true,i[p+"_RD"],i[p+"_VALUE"]};
}
}
int main() {
    try {
        replay("RV32IPredictionCounterTest",[](auto){return [](U i){return U{{"NEXT",next_counter(i["CURRENT"],i["TAKEN"],i["ONE_BIT"])}};};});
        replay("RV32IBranchPredecodeTest",[](auto){return [](U i){auto d=predecode(i["PC"],i["INSTRUCTION"]);
            return U{{"CONDITIONAL",d.conditional},{"DIRECT_JUMP",d.direct_jump},{"BACKWARD",d.backward},{"TARGET",d.target},{"PC_PLUS_4",d.pc_plus_4}};};});
        replay("RV32IBranchPredictorTest",[](auto p){return [m=Predictor(policy(p),rv32i::predictionConfig(p).entries)](U i) mutable {
            m.evaluate(i["CLK"],i["RST"],i["UPDATE"],i["UPDATE_PC"],i["UPDATE_TAKEN"]);
            const auto o=m.lookup(i["PC"],{bool(i["CONDITIONAL"]),bool(i["DIRECT_JUMP"]),bool(i["BACKWARD"]),i["TARGET"],i["PC_PLUS_4"]});
            return U{{"NEXT_PC",o.next_pc},{"TAKEN",o.taken},{"COUNTER",o.counter}};};});
        replay("RV32IBranchRecoveryTest",[](auto){return [](U i){return recoveryOutputs(recover(i["VALID"],i["PRETERMINAL"],i["ADVANCE"],i["KILL"],i["IS_BRANCH"],i["RST"],i["PREDICTED_PC"],i["FALLTHROUGH_PC"],i["TARGET_PC"],i["TAKEN"]));};});
        replay("RV32IPredictionPathTest",[](auto p){return [m=PredictionPath(policy(p),rv32i::predictionConfig(p).entries)](U i) mutable {
            m.evaluate(i["CLK"],i["RST"],i["IF_ID_WRITE"],i["ID_EX_WRITE"],i["FETCH_PC"],i["FETCH_INSTRUCTION"],
                i["EX_VALID"],i["EX_PRETERMINAL"],i["EX_ADVANCE"],i["EX_KILL"],i["EX_IS_BRANCH"],i["EX_PC"],i["EX_TAKEN"]);
            auto o=recoveryOutputs(recover(i["EX_VALID"],i["EX_PRETERMINAL"],i["EX_ADVANCE"],i["EX_KILL"],i["EX_IS_BRANCH"],i["RST"],m.id_ex_prediction,i["EX_FALLTHROUGH_PC"],i["EX_TARGET_PC"],i["EX_CONTROL_TAKEN"]));
            o["PREDICTED_NEXT_PC"]=m.predictor.lookup(i["FETCH_PC"],predecode(i["FETCH_PC"],i["FETCH_INSTRUCTION"])).next_pc;
            o["FETCH_NEXT_PC"]=i["REDIRECT_APPLY"]?(i["EX_CONTROL_TAKEN"]?i["EX_TARGET_PC"]:i["EX_FALLTHROUGH_PC"]):o["PREDICTED_NEXT_PC"];
            o["EX_PREDICTED_NEXT_PC"]=m.id_ex_prediction;return o;};});
        replay("RV32IForwardingUnitTest",[](auto){return [](U i){return U{
            {"FORWARD_A",forward_select(i["ID_EX_RS1"],producer(i,"EX_MEM"),producer(i,"MEM_WB"))},
            {"FORWARD_B",forward_select(i["ID_EX_RS2"],producer(i,"EX_MEM"),producer(i,"MEM_WB"))}};};});
        replay("RV32IHazardDetectionUnitTest",[](auto){return [](U i){auto h=load_use(i["ID_VALID"],i["ID_RS1"],i["ID_RS2"],i["ID_USES_RS1"],i["ID_USES_RS2"],i["EX_VALID"],i["EX_MEM_READ"],i["EX_RD"]);
            return U{{"LOAD_USE_HAZARD",h},{"STALL_FETCH",h},{"STALL_IF_ID",h},{"BUBBLE_ID_EX",h}};};});
        replay("RV32IPipelineCoordinatorTest",[](auto){return [](U i){const auto r=control({bool(i["ENABLE"]),bool(i["HALTED"]),bool(i["TRAPPED"]),
            bool(i["IMEM_READY"]),bool(i["DMEM_READY"]),bool(i["LOAD_USE_HAZARD"]),bool(i["TERMINAL_PENDING"]),bool(i["EX_PRETERMINAL"]),
            bool(i["MEMORY_TERMINAL"]),bool(i["WB_TERMINAL"]),bool(i["EX_REDIRECT"]),bool(i["MEMORY_FAULT"]),bool(i["MEM_LOAD"]),bool(i["MEM_STORE"]),bool(i["RETIRE_TERMINAL"])});
            return U{{"ACTIVE",r.active},{"IMEM_READ_ENABLE",r.imem_read},{"DMEM_READ_ENABLE",r.dmem_read},{"DMEM_WRITE_ENABLE",r.dmem_write},
                {"FETCH_RESPONSE_TAKE",r.fetch_response_take},{"DATA_REQUEST_ISSUE",r.data_request_issue},
                {"FETCH_VALID",r.fetch_valid},{"FETCH_PC_WRITE",r.pc_write},{"FETCH_PC_REDIRECT",r.pc_redirect},{"IF_ID_WRITE",r.if_id_write},{"IF_ID_FLUSH",r.if_id_flush},
                {"ID_EX_WRITE",r.id_ex_write},{"ID_EX_FLUSH",r.id_ex_flush},{"EX_MEM_WRITE",r.ex_mem_write},{"EX_MEM_FLUSH",r.ex_mem_flush},
                {"MEM_WB_WRITE",r.mem_wb_write},{"MEM_WB_FLUSH",r.mem_wb_flush},{"COMMIT_ENABLE",r.commit},{"PIPELINE_STALL",r.stall},
                {"LOAD_USE_STALL",r.load_use_stall},{"MEMORY_STALL",r.memory_stall},{"DATA_PORT_STALL",r.port_stall},{"PIPELINE_FLUSH",r.flush}};};});
        for(auto p:{circuit::ParameterMap{{"prediction","unknown"}},circuit::ParameterMap{{"prediction_entries","0"}},
                    circuit::ParameterMap{{"prediction_entries","3"}},circuit::ParameterMap{{"prediction_entries","257"}},
                    circuit::ParameterMap{{"prediction_entries","-1"}},circuit::ParameterMap{{"prediction_entries","999999999999999999999999"}}})
            if(rv32i::supportsPredictionParameters(p)) throw std::runtime_error("accepted invalid predictor configuration");
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
