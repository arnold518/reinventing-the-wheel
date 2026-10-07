#include "analysis/RV32IWorkloads.hpp"
#include "rv32i/RV32IInstructionOracle.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>

// Budget planning only, NOT a circuit performance measurement. No trace retained.
// Schedule the oracle's instruction stream using the zero-wait baseline's
// no-forwarding scoreboard and EX control-flow resolution.
void estimate(const circuit::analysis::RV32IWorkloadRequest& request, uint64_t limit) {
    using namespace rv32i;
    using namespace circuit::analysis;
    auto prepared = prepareRV32IWorkload(request);
    RV32IFunctionalMemory imem(request.capacity_bytes), dmem(request.capacity_bytes);
    for (const auto& b : prepared.blocks)
        (b.instruction ? imem : dmem).loadBytes(b.address, b.bytes);
    RV32IState state;
    std::array<uint64_t, 32> readable{};
    uint64_t next_decode = 2, final_decode = 0, stalls = 0, controls = 0, instructions = 0;
    bool done = false;
    for (uint64_t i = 0; i < limit; ++i) {
        auto trace = RV32IInstructionOracle::step(state, imem, dmem);
        const auto& c = trace.control;
        uint64_t decode = next_decode;
        if (c.legal && !c.halt && !c.trap) {
            if (c.uses_rs1 && trace.rs1) decode = std::max(decode, readable[trace.rs1]);
            if (c.uses_rs2 && trace.rs2) decode = std::max(decode, readable[trace.rs2]);
        }
        stalls += decode - next_decode;
        ++instructions;
        final_decode = decode;
        if (c.reg_write && trace.rd) readable[trace.rd] = decode + 4;
        bool control = c.branch != RV32IBranchType::None || c.jump != RV32IJumpType::None;
        controls += control;
        next_decode = decode + (control ? 3 : 1);
        if (prepared.completion == RV32IWorkloadCompletion::TerminalState)
            done = state.halted || state.trapped;
        else if (trace.memory.kind == RV32IMemoryAccessKind::Write && !trace.memory.fault
                 && trace.memory.address == prepared.tohost) {
            auto verdict = dmem.readU32(prepared.tohost);
            if (verdict == 3) throw std::runtime_error("ELF self-check failed");
            done = verdict == 1;
        }
        if (done) break;
        if (state.halted || state.trapped)
            throw std::runtime_error("Unexpected terminal instruction at PC " + std::to_string(state.pc));
    }
    if (!done) throw std::runtime_error("Oracle instruction budget exceeded");
    std::cout << "{\"workload\":\"" << prepared.descriptor.id
              << "\",\"fingerprint\":\"" << prepared.fingerprint
              << "\",\"instructions\":" << instructions
              << ",\"baseline_cycles_estimate\":" << final_decode + 3
              << ",\"data_stall_cycles\":" << stalls
              << ",\"control_flow_instructions\":" << controls << "}\n";
}

int main(int argc, char** argv) {
    try {
        circuit::analysis::RV32IWorkloadRequest request;
        uint64_t limit = 2000000000ULL;
        bool selected = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (i + 1 >= argc) throw std::runtime_error("Missing argument value");
            std::string value = argv[++i];
            if (arg == "--elf") {request.elf = value; selected = true;}
            else if (arg == "--program") {request.program = std::stoull(value); selected = true;}
            else if (arg == "--max-instructions") limit = std::stoull(value);
            else throw std::runtime_error("Unknown argument: " + arg);
        }
        if (selected) estimate(request, limit);
        else for (size_t i = 1; i <= 22; ++i) {request.program = i; estimate(request, limit);}
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
