#pragma once
// Host-side workloads for the chapter 6 accelerator: integer layers, their
// independent reference results, and the MatrixUnit command lists that compute
// them. The guest driver (tools/accelerator/guest) builds the same commands in C.
#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace circuit::accelerator::test {

struct LayerShape {
    std::string name;
    size_t rows, inputs, outputs;     // X is rows × inputs, W is inputs × outputs
    uint16_t scale;
    unsigned shift;
    bool relu;
};
struct Layer {
    LayerShape shape;
    std::vector<int8_t> x, w;         // row-major
    std::vector<int32_t> bias;
};
// 3×3 convolution, no padding, HWC int8 image, W[filter][ky][kx][channel].
struct ConvShape {
    std::string name;
    size_t height, width, channels, filters;
    uint16_t scale;
    unsigned shift;
    bool relu;
    size_t stride = 1;
    size_t outHeight() const { return (height - 3) / stride + 1; }
    size_t outWidth() const { return (width - 3) / stride + 1; }
};
struct Convolution {
    ConvShape shape;
    std::vector<int8_t> image, w;
    std::vector<int32_t> bias;
};

Layer randomLayer(const LayerShape&, uint64_t seed);
Convolution randomConvolution(const ConvShape&, uint64_t seed);
int8_t requantize(int64_t acc, uint16_t scale, unsigned shift, bool relu);
std::vector<uint8_t> referenceLayer(const Layer&);
std::vector<uint8_t> referenceConvolution(const Convolution&);

struct AcceleratorCommand { uint32_t op; std::array<uint32_t, 4> args; };
struct AcceleratorProgram {
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> memory;   // address, bytes
    std::vector<AcceleratorCommand> commands;
    uint32_t output_address = 0;
    size_t output_bytes = 0;
};
// Buffer capacities of the unit the program is built for.
struct UnitCapacity { size_t act_rows = 256, acc_rows = 64; };
AcceleratorProgram layerProgram(const Layer&, size_t n, uint32_t base, UnitCapacity = {64, 16});
AcceleratorProgram convolutionProgram(const Convolution&, size_t n, uint32_t base, UnitCapacity = {64, 16});

} // namespace circuit::accelerator::test
