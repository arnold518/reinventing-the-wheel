#pragma once
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <cstdint>

// The RV32I execute ALU: 32 bits, parallel-prefix addition, no flags.
namespace rv32i::ALUOp {
inline constexpr uint8_t ADD = 0, SUB = 1, AND = 2, OR = 3, XOR = 4;
inline constexpr uint8_t SLL = 5, SRL = 6, SRA = 7, SLT = 8, SLTU = 9;
inline constexpr uint8_t PASS_A = 10, PASS_B = 11, CLEAR = 12;  // 13-31 give 0
}

#define CIRCUITSIM_RV32I_ALU_PART(Class)                                      \
    class Class : public IOComponent {                                        \
    public:                                                                   \
        explicit Class(std::string name);                                     \
        static constexpr const char* TypeName = #Class;                      \
        const char* getTypeName() const override { return TypeName; }         \
        void buildInternals(ComponentBuilder& builder) override;              \
    };

// A[32], B[32], OP[5] -> OUT[32], EQ, LT_SIGNED, LT_UNSIGNED.
// The comparisons are valid only for SUB, SLT and SLTU; otherwise 0.
CIRCUITSIM_RV32I_ALU_PART(RV32IALU)
// OP[5] -> LOCAL_OP[3], HIGH_HALF, OUT_OF_RANGE, SUB, SHIFT_LEFT, SHIFT_ARITHMETIC.
CIRCUITSIM_RV32I_ALU_PART(RV32IALUControl)
// A, B, SUB -> OUT, C_OUT, OVERFLOW; SUB also supplies the carry-in.
CIRCUITSIM_RV32I_ALU_PART(RV32IALUAddSub)
// A, B -> AND_OUT, OR_OUT, XOR_OUT, all computed at once.
CIRCUITSIM_RV32I_ALU_PART(RV32IALULogic)
// A, B, LEFT, ARITHMETIC -> OUT, shifted by B[4:0].
CIRCUITSIM_RV32I_ALU_PART(RV32IALUShifter)
// DIFF, CARRY_IN, OVERFLOW_IN, ENABLE -> EQ, LT_SIGNED, LT_UNSIGNED, SLT_RESULT, SLTU_RESULT.
CIRCUITSIM_RV32I_ALU_PART(RV32IALUCompare)
// Seven results, A, B, LOCAL_OP, HIGH_HALF, OUT_OF_RANGE -> OUT.
CIRCUITSIM_RV32I_ALU_PART(RV32IALUResultSelect)
#undef CIRCUITSIM_RV32I_ALU_PART

namespace circuit { class ComponentCatalog; }
namespace circuit::families {
const ComponentFamily& rv32iALU();
const ComponentFamily& rv32iALUControl();
const ComponentFamily& rv32iALUAddSub();
const ComponentFamily& rv32iALULogic();
const ComponentFamily& rv32iALUShifter();
const ComponentFamily& rv32iALUCompare();
const ComponentFamily& rv32iALUResultSelect();
}
namespace rv32i {
void registerALU(circuit::ComponentCatalog& catalog);
}
