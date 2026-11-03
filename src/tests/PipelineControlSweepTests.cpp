#include "tests/PipelineControlSweepTests.hpp"
#include "tests/SettledSweep.hpp"
#include "modules/rv32i/RV32IPipelineControl.hpp"
#include <array>
#include <iostream>

namespace {
using namespace circuit::test;

// Register numbers: x0, and pairs that differ in one high or low bit.
constexpr std::array<uint64_t, 4> registers{0, 1, 17, 30};

// Independent rule for one operand: the newest valid writer of a nonzero
// register wins; a matching EX writer that is not ready blocks forwarding.
uint64_t forwardSelect(uint64_t rs, uint64_t ex_rd, bool ex_valid, bool ex_write, bool ex_ready,
                       uint64_t mem_rd, bool mem_valid, bool mem_write) {
    if (rs == 0) return 0;
    if (ex_valid && ex_write && ex_rd == rs) return ex_ready ? 1 : 0;
    if (mem_valid && mem_write && mem_rd == rs) return 2;
    return 0;
}

void sweepForwarding() {
    FidelitySweep sweep(circuit::families::RV32IForwardingUnit);
    auto check = [&](uint64_t rs1, uint64_t rs2, uint64_t ex_rd, uint64_t mem_rd, unsigned flags) {
        const bool ex_valid = flags & 1, ex_write = flags & 2, ex_ready = flags & 4;
        const bool mem_valid = flags & 8, mem_write = flags & 16;
        const auto inputs = sweep.gates().binary({{"ID_EX_RS1", rs1}, {"ID_EX_RS2", rs2},
            {"EX_MEM_RD", ex_rd}, {"EX_MEM_VALID", ex_valid}, {"EX_MEM_REG_WRITE", ex_write},
            {"EX_MEM_RESULT_READY", ex_ready}, {"MEM_WB_RD", mem_rd}, {"MEM_WB_VALID", mem_valid},
            {"MEM_WB_REG_WRITE", mem_write}});
        const auto expected = sweep.gates().binary({
            {"FORWARD_A", forwardSelect(rs1, ex_rd, ex_valid, ex_write, ex_ready, mem_rd, mem_valid, mem_write)},
            {"FORWARD_B", forwardSelect(rs2, ex_rd, ex_valid, ex_write, ex_ready, mem_rd, mem_valid, mem_write)}});
        sweep.evaluate(inputs, &expected);
    };
    for (auto rs1 : registers) for (auto rs2 : registers) for (auto ex_rd : registers) for (auto mem_rd : registers)
        for (unsigned flags = 0; flags < 32; ++flags) check(rs1, rs2, ex_rd, mem_rd, flags);
    // A destination that differs from the source in exactly one bit never matches.
    for (uint64_t rs = 1; rs < 32; ++rs) for (unsigned bit = 0; bit < 5; ++bit) {
        const uint64_t other = rs ^ (uint64_t{1} << bit);
        check(rs, rs, other, other, 31);
        check(rs, other, rs, other, 31);
    }
    std::cout << "RV32IForwardingUnit: " << sweep.vectors() << " vectors against the forwarding rule; "
              << (sweep.hasBehavioral() ? "gate-level and behavioral forms agree" : "gate-level form only") << '\n';
}

void sweepHazard() {
    FidelitySweep sweep(circuit::families::RV32IHazardDetectionUnit);
    auto check = [&](uint64_t rs1, uint64_t rs2, uint64_t ex_rd, unsigned flags) {
        const bool id_valid = flags & 1, uses_rs1 = flags & 2, uses_rs2 = flags & 4;
        const bool ex_valid = flags & 8, ex_mem_read = flags & 16;
        const bool hazard = id_valid && ex_valid && ex_mem_read && ex_rd != 0 &&
            ((uses_rs1 && rs1 == ex_rd) || (uses_rs2 && rs2 == ex_rd));
        const auto inputs = sweep.gates().binary({{"ID_VALID", id_valid}, {"ID_RS1", rs1}, {"ID_RS2", rs2},
            {"ID_USES_RS1", uses_rs1}, {"ID_USES_RS2", uses_rs2}, {"EX_VALID", ex_valid},
            {"EX_MEM_READ", ex_mem_read}, {"EX_RD", ex_rd}});
        const auto expected = sweep.gates().binary({{"LOAD_USE_HAZARD", hazard}, {"STALL_FETCH", hazard},
            {"STALL_IF_ID", hazard}, {"BUBBLE_ID_EX", hazard}});
        sweep.evaluate(inputs, &expected);
    };
    for (auto rs1 : registers) for (auto rs2 : registers) for (auto ex_rd : registers)
        for (unsigned flags = 0; flags < 32; ++flags) check(rs1, rs2, ex_rd, flags);
    for (uint64_t rd = 1; rd < 32; ++rd) for (unsigned bit = 0; bit < 5; ++bit) {
        const uint64_t other = rd ^ (uint64_t{1} << bit);
        check(other, other, rd, 31);
        check(rd, other, rd, 31);
    }
    std::cout << "RV32IHazardDetectionUnit: " << sweep.vectors() << " vectors against the load-use rule; "
              << (sweep.hasBehavioral() ? "gate-level and behavioral forms agree" : "gate-level form only") << '\n';
}
} // namespace

void RV32IPipelineControlSweepTest::verifyResults() {
    sweepForwarding();
    sweepHazard();
}
