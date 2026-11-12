#include "tests/AcceleratorWorkloads.hpp"
#include <algorithm>
#include <random>
#include <stdexcept>

namespace circuit::accelerator::test {

int8_t requantize(int64_t acc, uint16_t scale, unsigned shift, bool relu) {
    // The device accumulates in int32 (wrapping), then scales a 48-bit product.
    __int128 v = __int128(int32_t(uint32_t(acc))) * scale;
    if (shift) v += __int128(1) << (shift - 1);
    v >>= shift;
    v = std::clamp<__int128>(v, -128, 127);
    if (relu && v < 0) v = 0;
    return int8_t(v);
}

Layer randomLayer(const LayerShape& s, uint64_t seed) {
    std::mt19937_64 rng(seed);
    Layer l{s, std::vector<int8_t>(s.rows * s.inputs), std::vector<int8_t>(s.inputs * s.outputs), std::vector<int32_t>(s.outputs)};
    for (auto& v : l.x) v = int8_t(rng());
    for (auto& v : l.w) v = int8_t(rng());
    for (auto& v : l.bias) v = int32_t(rng() % 20001) - 10000;
    return l;
}
Convolution randomConvolution(const ConvShape& s, uint64_t seed) {
    std::mt19937_64 rng(seed);
    Convolution c{s, std::vector<int8_t>(s.height * s.width * s.channels), std::vector<int8_t>(s.filters * 9 * s.channels),
                  std::vector<int32_t>(s.filters)};
    for (auto& v : c.image) v = int8_t(rng());
    for (auto& v : c.w) v = int8_t(rng());
    for (auto& v : c.bias) v = int32_t(rng() % 20001) - 10000;
    return c;
}

std::vector<uint8_t> referenceLayer(const Layer& l) {
    const auto& s = l.shape;
    std::vector<uint8_t> y(s.rows * s.outputs);
    for (size_t m = 0; m < s.rows; ++m)
        for (size_t o = 0; o < s.outputs; ++o) {
            int64_t acc = l.bias[o];
            for (size_t k = 0; k < s.inputs; ++k) acc += int64_t(l.x[m * s.inputs + k]) * l.w[k * s.outputs + o];
            y[m * s.outputs + o] = uint8_t(requantize(acc, s.scale, s.shift, s.relu));
        }
    return y;
}
std::vector<uint8_t> referenceConvolution(const Convolution& c) {
    const auto& s = c.shape;
    const size_t oh = s.outHeight(), ow = s.outWidth(), ch = s.channels;
    std::vector<uint8_t> y(oh * ow * s.filters);
    for (size_t oy = 0; oy < oh; ++oy)
        for (size_t ox = 0; ox < ow; ++ox)
            for (size_t f = 0; f < s.filters; ++f) {
                int64_t acc = c.bias[f];
                for (size_t ky = 0; ky < 3; ++ky)
                    for (size_t kx = 0; kx < 3; ++kx)
                        for (size_t k = 0; k < ch; ++k)
                            acc += int64_t(c.image[((oy * s.stride + ky) * s.width + ox * s.stride + kx) * ch + k]) *
                                   c.w[((f * 3 + ky) * 3 + kx) * ch + k];
                y[(oy * ow + ox) * s.filters + f] = uint8_t(requantize(acc, s.scale, s.shift, s.relu));
            }
    return y;
}

namespace {
constexpr uint32_t LOAD = 1, STORE = 2, SHIFT = 3, MATMUL = 4, ACTIVATE = 5;
constexpr uint32_t TO_ACTIVATIONS = 0, TO_WEIGHTS = 1u << 30, TO_BIAS = 2u << 30;

// The part common to dense layers and convolutions: `inputs` values per
// patch in K = `chunks` buffer rows, a weight matrix wk(k, o), and per-group
// gathers supplied by the caller.
struct Builder {
    AcceleratorProgram p;
    uint32_t next;
    explicit Builder(uint32_t base) : next(base) {}
    uint32_t place(std::vector<uint8_t> bytes) {
        const uint32_t at = next;
        next = (next + uint32_t(bytes.size()) + 16 + 3) & ~3u;   // a little slack after each block
        p.memory.emplace_back(at, std::move(bytes));
        return at;
    }
    void command(uint32_t op, uint32_t a0 = 0, uint32_t a1 = 0, uint32_t a2 = 0, uint32_t a3 = 0) {
        p.commands.push_back({op, {a0, a1, a2, a3}});
    }
};

// Weight tiles: chunk c, output tile t → N rows (inputs c·N + r) of N bytes (outputs t·N + j).
template<class WeightAt>
uint32_t placeTiles(Builder& b, size_t n, size_t inputs, size_t outputs, WeightAt weight) {
    const size_t chunks = (inputs + n - 1) / n, tiles = (outputs + n - 1) / n;
    std::vector<uint8_t> bytes(chunks * tiles * n * n, 0);
    for (size_t t = 0; t < tiles; ++t)
        for (size_t c = 0; c < chunks; ++c)
            for (size_t r = 0; r < n; ++r)
                for (size_t j = 0; j < n; ++j) {
                    const size_t k = c * n + r, o = t * n + j;
                    if (k < inputs && o < outputs) bytes[((t * chunks + c) * n + r) * n + j] = uint8_t(weight(k, o));
                }
    return b.place(std::move(bytes));
}
uint32_t placeBias(Builder& b, size_t n, const std::vector<int32_t>& bias) {
    const size_t tiles = (bias.size() + n - 1) / n;
    std::vector<uint8_t> bytes(tiles * n * 4, 0);
    for (size_t o = 0; o < bias.size(); ++o)
        for (int k = 0; k < 4; ++k) bytes[o * 4 + k] = uint8_t(uint32_t(bias[o]) >> (8 * k));
    return b.place(std::move(bytes));
}

// For each group of `count` patches already gathered into activation rows
// 0 .. count·chunks: every output tile's bias, weights, multiply, activate, store.
void computeGroup(Builder& b, size_t n, size_t chunks, size_t outputs, uint32_t tiles_at, uint32_t bias_at,
                  size_t count, uint32_t out_address, size_t out_stride, uint16_t scale, unsigned shift, bool relu) {
    const size_t tiles = (outputs + n - 1) / n;
    const uint32_t out_row = uint32_t(count * chunks);
    for (size_t t = 0; t < tiles; ++t) {
        const size_t columns = std::min(n, outputs - t * n);
        b.command(LOAD, bias_at + uint32_t(t * n * 4), 1u << 16, (1u << 24) | uint32_t(4 * n) << 16, TO_BIAS);
        for (size_t c = 0; c < chunks; ++c) {
            b.command(LOAD, tiles_at + uint32_t((t * chunks + c) * n * n), 1u << 16,
                      uint32_t(n) << 24 | uint32_t(n) << 16 | uint32_t(n), TO_WEIGHTS);
            b.command(SHIFT);
            b.command(MATMUL, uint32_t(count) << 16 | uint32_t(c), uint32_t(chunks), c ? 1u : 0u);
        }
        b.command(ACTIVATE, uint32_t(count) << 16, out_row, (relu ? 1u << 24 : 0u) | shift << 16 | scale);
        b.command(STORE, out_address + uint32_t(t * n), uint32_t(count) << 16 | out_row,
                  1u << 24 | uint32_t(columns) << 16, 1u << 16 | uint32_t(out_stride));
    }
}
size_t groupSize(size_t chunks, UnitCapacity cap) {
    const size_t g = std::min(cap.acc_rows, cap.act_rows / (chunks + 1));
    if (g == 0) throw std::invalid_argument("Patch does not fit the activation buffer");
    return std::min<size_t>(g, 0xffff);
}
} // namespace

AcceleratorProgram layerProgram(const Layer& l, size_t n, uint32_t base, UnitCapacity cap) {
    const auto& s = l.shape;
    Builder b(base);
    const size_t chunks = (s.inputs + n - 1) / n;
    if (chunks > 255) throw std::invalid_argument("Layer too wide for one gather");
    const uint32_t bias_at = placeBias(b, n, l.bias);
    const uint32_t tiles_at = placeTiles(b, n, s.inputs, s.outputs, [&](size_t k, size_t o) { return l.w[k * s.outputs + o]; });
    std::vector<uint8_t> x(l.x.begin(), l.x.end());
    x.resize(x.size() + chunks * n, 0);                  // the last row's gather reads past the end
    const uint32_t x_at = b.place(std::move(x));
    b.p.output_bytes = s.rows * s.outputs;
    b.p.output_address = b.place(std::vector<uint8_t>(b.p.output_bytes, 0));
    const size_t g = groupSize(chunks, cap);
    for (size_t first = 0; first < s.rows; first += g) {
        const size_t count = std::min(g, s.rows - first);
        // Each row: `chunks` rows of N bytes from consecutive memory.
        b.command(LOAD, x_at + uint32_t(first * s.inputs), uint32_t(count) << 16,
                  uint32_t(chunks) << 24 | uint32_t(n) << 16 | uint32_t(n), uint32_t(chunks) << 16 | uint32_t(s.inputs) | TO_ACTIVATIONS);
        computeGroup(b, n, chunks, s.outputs, tiles_at, bias_at, count, b.p.output_address + uint32_t(first * s.outputs),
                     s.outputs, s.scale, s.shift, s.relu);
    }
    return b.p;
}

AcceleratorProgram convolutionProgram(const Convolution& c, size_t n, uint32_t base, UnitCapacity cap) {
    const auto& s = c.shape;
    Builder b(base);
    const size_t inputs = 9 * s.channels, chunks = (inputs + n - 1) / n;
    const size_t oh = s.outHeight(), ow = s.outWidth();
    const uint32_t bias_at = placeBias(b, n, c.bias);
    const uint32_t tiles_at = placeTiles(b, n, inputs, s.filters, [&](size_t k, size_t o) { return c.w[o * inputs + k]; });
    const uint32_t image_at = b.place(std::vector<uint8_t>(c.image.begin(), c.image.end()));
    b.p.output_bytes = oh * ow * s.filters;
    b.p.output_address = b.place(std::vector<uint8_t>(b.p.output_bytes, 0));
    const size_t g = groupSize(chunks, cap), patches = oh * ow;
    for (size_t first = 0; first < patches; first += g) {
        const size_t count = std::min(g, patches - first);
        // Gather: one LOAD per run of patches within one output row (3 rows of 3·C bytes each).
        for (size_t q = first; q < first + count;) {
            const size_t oy = q / ow, ox = q % ow, run = std::min(ow - ox, first + count - q);
            const uint32_t src = image_at + uint32_t(((oy * s.stride) * s.width + ox * s.stride) * s.channels);
            b.command(LOAD, src, uint32_t(run) << 16 | uint32_t((q - first) * chunks),
                      3u << 24 | uint32_t(3 * s.channels) << 16 | uint32_t(s.width * s.channels),
                      uint32_t(chunks) << 16 | uint32_t(s.stride * s.channels) | TO_ACTIVATIONS);
            q += run;
        }
        computeGroup(b, n, chunks, s.filters, tiles_at, bias_at, count, b.p.output_address + uint32_t(first * s.filters),
                     s.filters, s.scale, s.shift, s.relu);
    }
    return b.p;
}

} // namespace circuit::accelerator::test
