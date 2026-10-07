#include "modules/rv32i/RV32IPipelineControl.hpp"

#include "components/ComponentBuilder.hpp"
#include "components/ComponentBuilder.tpp"
#include "RV32ILogicNetwork.hpp"

namespace {
using rv32i::detail::LogicNetwork;
using rv32i::detail::NetId;
} // namespace

RV32IPipelineCoordinator::RV32IPipelineCoordinator(
    std::string name)
    : IOComponent(
          std::move(name),
          circuit::families::RV32IPipelineCoordinator
              .pinInitializer()) {}

void RV32IPipelineCoordinator::buildInternals(
    ComponentBuilder& builder) {
    LogicNetwork logic(builder);
    const auto input = [&](const char* name) {
        return logic.source(name, getInputPin(name));
    };
    const auto enable = input("ENABLE");
    const auto halted = input("HALTED");
    const auto trapped = input("TRAPPED");
    const auto imem_ready = input("IMEM_READY");
    const auto dmem_ready = input("DMEM_READY");
    const auto hazard = input("LOAD_USE_HAZARD");
    const auto terminal_pending = input("TERMINAL_PENDING");
    const auto ex_preterminal = input("EX_PRETERMINAL");
    const auto memory_terminal = input("MEMORY_TERMINAL");
    const auto wb_terminal = input("WB_TERMINAL");
    const auto ex_redirect = input("EX_REDIRECT");
    const auto memory_fault = input("MEMORY_FAULT");
    const auto mem_load = input("MEM_LOAD");
    const auto mem_store = input("MEM_STORE");
    const auto retire_terminal = input("RETIRE_TERMINAL");

    const auto active = logic.andAll(
        "ACTIVE",
        {enable,
         logic.logicalNot("NOT_HALTED", halted),
         logic.logicalNot("NOT_TRAPPED", trapped)});
    const auto not_wb_terminal =
        logic.logicalNot("NOT_WB_TERMINAL", wb_terminal);
    const auto memory_access = logic.logicalAnd(
        "MEMORY_ACCESS",
        logic.logicalOr("MEMORY_ACCESS_RAW", mem_load, mem_store),
        not_wb_terminal);
    const auto not_dmem_ready =
        logic.logicalNot("NOT_DMEM_READY", dmem_ready);
    const auto memory_wait = logic.andAll(
        "MEMORY_WAIT",
        {active, memory_access, not_dmem_ready});
    const auto port_conflict = logic.andAll(
        "DATA_PORT_CONFLICT",
        {active, mem_store, mem_load, dmem_ready,
         not_wb_terminal});
    const auto not_memory_wait =
        logic.logicalNot("NOT_MEMORY_WAIT", memory_wait);
    const auto not_port_conflict =
        logic.logicalNot("NOT_PORT_CONFLICT", port_conflict);
    const auto not_retire_terminal =
        logic.logicalNot(
            "NOT_RETIRE_TERMINAL", retire_terminal);
    const auto terminal_step = logic.andAll(
        "TERMINAL_STEP",
        {active, not_memory_wait, retire_terminal});
    const auto normal_step = logic.andAll(
        "NORMAL_STEP",
        {active,
         not_memory_wait,
         not_port_conflict,
         not_retire_terminal});
    const auto kill = logic.orAll(
        "KILL_YOUNGER",
        {ex_preterminal, ex_redirect, memory_fault});
    const auto not_kill =
        logic.logicalNot("NOT_KILL", kill);
    const auto not_hazard =
        logic.logicalNot("NOT_HAZARD", hazard);
    const auto any_terminal_pending = logic.orAll(
        "ANY_TERMINAL_PENDING",
        {terminal_pending,
         ex_preterminal,
         memory_terminal,
         wb_terminal});
    const auto not_terminal_pending = logic.logicalNot(
        "NOT_TERMINAL_PENDING", any_terminal_pending);
    const auto fetch_valid = logic.andAll(
        "FETCH_VALID",
        {normal_step,
         not_kill,
         not_hazard,
         imem_ready,
         not_terminal_pending});
    const auto redirect_step = logic.logicalAnd(
        "REDIRECT_STEP", normal_step, ex_redirect);

    const auto output = [&](NetId net, const char* name) {
        logic.sink(net, getOutputPin(name));
    };
    output(active, "ACTIVE");
    output(
        logic.logicalAnd(
            "IMEM_READ_ENABLE",
            active,
            not_terminal_pending),
        "IMEM_READ_ENABLE");
    const auto dmem_read = logic.andAll(
        "DMEM_READ_ENABLE",
        {active, mem_load, logic.logicalNot("NOT_MEM_STORE", mem_store), not_wb_terminal});
    const auto dmem_write = logic.andAll(
        "DMEM_WRITE_ENABLE", {active, mem_store, not_wb_terminal});
    output(dmem_read, "DMEM_READ_ENABLE");
    output(dmem_write, "DMEM_WRITE_ENABLE");
    output(logic.logicalOr("DATA_ISSUE", dmem_read, dmem_write), "DATA_REQUEST_ISSUE");
    output(fetch_valid, "FETCH_VALID");
    output(
        logic.logicalOr(
            "FETCH_PC_WRITE", fetch_valid, redirect_step),
        "FETCH_PC_WRITE");
    output(redirect_step, "FETCH_PC_REDIRECT");
    output(
        logic.logicalOr(
            "IF_ID_WRITE",
            terminal_step,
            logic.logicalAnd(
                "NORMAL_IF_ID_WRITE",
                normal_step,
                not_hazard)),
        "IF_ID_WRITE");
    const auto ifid_flush = logic.logicalOr(
        "IF_ID_FLUSH", terminal_step,
        logic.logicalAnd("NORMAL_IF_ID_FLUSH", normal_step, kill));
    output(ifid_flush, "IF_ID_FLUSH");
    // Preserve the former root-level OR chain and its propagation depth.
    // Consumption also drains a response after halt/trap, even while paused.
    const auto stopped = logic.logicalOr("FETCH_STOPPED", halted, trapped);
    const auto discard = logic.logicalOr("FETCH_DISCARD", stopped, ifid_flush);
    output(logic.logicalOr("FETCH_CONSUME", fetch_valid, discard), "FETCH_RESPONSE_TAKE");
    output(
        logic.logicalOr(
            "ID_EX_WRITE", terminal_step, normal_step),
        "ID_EX_WRITE");
    output(
        logic.logicalOr(
            "ID_EX_FLUSH",
            terminal_step,
            logic.logicalAnd(
                "NORMAL_ID_EX_FLUSH",
                normal_step,
                logic.logicalOr(
                    "KILL_OR_HAZARD", kill, hazard))),
        "ID_EX_FLUSH");
    output(
        logic.logicalOr(
            "EX_MEM_WRITE", terminal_step, normal_step),
        "EX_MEM_WRITE");
    output(
        logic.logicalOr(
            "EX_MEM_FLUSH",
            terminal_step,
            logic.logicalAnd(
                "NORMAL_EX_MEM_FLUSH",
                normal_step,
                memory_fault)),
        "EX_MEM_FLUSH");
    output(
        logic.orAll(
            "MEM_WB_WRITE",
            {terminal_step, port_conflict, normal_step}),
        "MEM_WB_WRITE");
    output(
        logic.logicalOr(
            "MEM_WB_FLUSH", terminal_step, port_conflict),
        "MEM_WB_FLUSH");
    output(
        logic.logicalAnd(
            "COMMIT_ENABLE", active, not_memory_wait),
        "COMMIT_ENABLE");
    output(
        logic.orAll(
            "PIPELINE_STALL",
            {memory_wait, port_conflict, hazard}),
        "PIPELINE_STALL");
    output(
        logic.logicalAnd(
            "LOAD_USE_STALL", normal_step, hazard),
        "LOAD_USE_STALL");
    output(memory_wait, "MEMORY_STALL");
    output(port_conflict, "DATA_PORT_STALL");
    output(
        logic.logicalAnd(
            "PIPELINE_FLUSH", normal_step, kill),
        "PIPELINE_FLUSH");
    logic.materialize();
}
