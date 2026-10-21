#include "rv32im/RV32IMInstruction.hpp"
#include <bit>
#include <limits>
#include <stdexcept>
namespace rv32im {
DecodedInstruction decode(uint32_t raw) {
    DecodedInstruction d{rv32i::RV32IDecoder::decode(raw), {}};
    if ((raw & 0xfe00007fu) == 0x02000033u)
        d.m = static_cast<MOperation>((raw >> 12) & 7u);
    return d;
}
const char* name(MOperation op) {
    switch (op) {
    case MOperation::MUL: return "MUL"; case MOperation::MULH: return "MULH";
    case MOperation::MULHSU: return "MULHSU"; case MOperation::MULHU: return "MULHU";
    case MOperation::DIV: return "DIV"; case MOperation::DIVU: return "DIVU";
    case MOperation::REM: return "REM"; case MOperation::REMU: return "REMU";
    }
    throw std::invalid_argument("Invalid RV32IM operation");
}
uint32_t referenceResult(MOperation op, uint32_t a, uint32_t b) {
    const int64_t sa = std::bit_cast<int32_t>(a), sb = std::bit_cast<int32_t>(b);
    switch (op) {
    case MOperation::MUL: return static_cast<uint32_t>(uint64_t(a) * b);
    case MOperation::MULH: return static_cast<uint32_t>(static_cast<uint64_t>(sa * sb) >> 32);
    case MOperation::MULHSU: return static_cast<uint32_t>(static_cast<uint64_t>(sa * int64_t(b)) >> 32);
    case MOperation::MULHU: return static_cast<uint32_t>((uint64_t(a) * b) >> 32);
    case MOperation::DIV: return b == 0 ? UINT32_MAX : static_cast<uint32_t>(sa / sb);
    case MOperation::DIVU: return b == 0 ? UINT32_MAX : a / b;
    case MOperation::REM: return b == 0 ? a : static_cast<uint32_t>(sa % sb);
    case MOperation::REMU: return b == 0 ? a : a % b;
    }
    throw std::invalid_argument("Invalid RV32IM operation");
}
InstructionTrace step(rv32i::RV32IState& s, rv32i::RV32IFunctionalMemory& im,
                      rv32i::RV32IFunctionalMemory& dm) {
    if (s.halted || s.trapped || (s.pc & 3u) || !im.canAccess(s.pc, 4))
        return {rv32i::RV32IInstructionOracle::step(s, im, dm), {}};
    const uint32_t raw = im.readU32(s.pc);
    const auto d = decode(raw);
    if (!d.m) return {rv32i::RV32IInstructionOracle::step(s, im, dm), {}};
    InstructionTrace t; t.m = d.m;
    auto& x = t.base;
    x.instruction_index = s.instruction_count;
    x.pc_before = s.pc; x.raw_instruction = raw; x.decoded = d.base;
    x.rs1 = (raw >> 15) & 31; x.rs2 = (raw >> 20) & 31; x.rd = (raw >> 7) & 31;
    x.rs1_value = s.readRegister(x.rs1); x.rs2_value = s.readRegister(x.rs2);
    x.alu_result = referenceResult(*d.m, x.rs1_value, x.rs2_value);
    x.writeback = {true, x.rd, x.alu_result, x.rd == 0};
    s.writeRegister(x.rd, x.alu_result); s.pc += 4; ++s.instruction_count; s.forceX0();
    x.pc_after = s.pc;
    return t;
}
}
