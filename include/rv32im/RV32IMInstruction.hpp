#pragma once
#include "rv32i/RV32IDecoder.hpp"
#include "rv32i/RV32IInstructionOracle.hpp"
#include <optional>
namespace rv32im {
enum class MOperation : uint8_t { MUL, MULH, MULHSU, MULHU, DIV, DIVU, REM, REMU };
struct DecodedInstruction {
    rv32i::RV32IDecodedInstruction base;
    std::optional<MOperation> m;
    bool legal() const { return m.has_value() || base.legal; }
};
DecodedInstruction decode(uint32_t raw);
const char* name(MOperation);
// Display decoding; RV32I decoding remains strict about unsupported M code.
std::string disassemble(uint32_t raw, uint32_t pc = 0);
// Independent architectural reference, never called by a circuit implementation.
uint32_t referenceResult(MOperation, uint32_t a, uint32_t b);
struct InstructionTrace {
    rv32i::RV32IInstructionTrace base;
    std::optional<MOperation> m;
};
InstructionTrace step(rv32i::RV32IState&, rv32i::RV32IFunctionalMemory& instructions,
                      rv32i::RV32IFunctionalMemory& data);
}
