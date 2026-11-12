#pragma once
// Chapter 6 accelerator device: a command queue, a DMA master, activation,
// weight and accumulator buffers, an N × N systolic array and N requantizers,
// on the chapter-5 register front end. See site-docs/accelerator/6.4.md.
#include "components/IOComponent.hpp"
#include "components/selection/ComponentFamily.hpp"
#include <string>

namespace circuit { class ComponentCatalog; }

namespace circuit::accelerator {

class MatrixUnit : public IOComponent {
public:
    MatrixUnit(std::string name, size_t size);
    const char* getTypeName() const override { return type_.c_str(); }
    void buildInternals(ComponentBuilder&) override;
    size_t size() const { return size_; }
private:
    size_t size_;
    std::string type_;
};
template<size_t N> class MatrixUnitOf : public MatrixUnit {
public:
    explicit MatrixUnitOf(std::string name) : MatrixUnit(std::move(name), N) {}
};

enum class MatrixCommand : uint32_t { Load = 1, Store = 2, ShiftWeights = 3, MatMul = 4, Activate = 5 };
bool supportsMatrixUnit(const ParameterMap&);
void registerMatrixUnits(ComponentCatalog&);
} // namespace circuit::accelerator

namespace circuit::families {
const ComponentFamily& matrixUnit(size_t n);   // device.matrix-unit.N, N = 4, 8, 16
}
