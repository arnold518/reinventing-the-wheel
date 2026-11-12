#pragma once
// Chapter 6 arithmetic: the int8 multiply-add cell, the requantizer and the
// weight-stationary systolic array built from the cells.
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <string>

namespace circuit { class ComponentCatalog; }

namespace circuit::accelerator {

// One weight-stationary cell. Each rising edge:
//   SUM_OUT <= SUM_IN + X_IN × WEIGHT   (int8 × int8 added into int32, wrapping)
//   X_OUT   <= X_IN
//   WEIGHT  <= W_IN when SHIFT = 1 (W_OUT shows WEIGHT)
// Reset clears all three registers.
class MacCell : public IOComponent {
public:
    explicit MacCell(std::string name);
    static constexpr const char* TypeName = "MacCell";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

// int32 → int8 (combinational):
//   OUT = clamp((ACC × SCALE + round) >> SHIFT, -128, 127), then 0 if RELU and negative,
// with SCALE unsigned, the shift arithmetic, and round = 2^(SHIFT-1) (0 for SHIFT = 0).
class Requantizer : public IOComponent {
public:
    explicit Requantizer(std::string name);
    static constexpr const char* TypeName = "Requantizer";
    const char* getTypeName() const override { return TypeName; }
    void buildInternals(ComponentBuilder&) override;
};

// N × N weight-stationary array with input skew and output de-skew. A vector
// on X_IN_0..X_IN_{N-1} before edge e appears as Y_OUT_j = Σ_i X_IN_i × W[i][j]
// (plus nothing else: the top sums are zero) after edge e + 2N - 2.
// Weights load by holding SHIFT = 1 for N edges with W_IN_j = W[N-1-k][j]
// at the k-th edge; row 0 receives W_IN and each row passes its weight down.
class SystolicArray : public IOComponent {
public:
    SystolicArray(std::string name, size_t size);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
    size_t size() const { return size_; }
private:
    size_t size_;
    std::string type_;
};
inline size_t systolicLatency(size_t n) { return 2 * n - 2; }

void registerMatrixParts(ComponentCatalog&);
} // namespace circuit::accelerator

namespace circuit::families {
extern const ComponentFamily MacCell;       // accelerator.mac-cell
extern const ComponentFamily Requantizer;   // accelerator.requantize
const ComponentFamily& systolicArray(size_t n);   // accelerator.systolic-array.N, N = 2, 4, 8, 16
}
