#include "matrix.h"
#include "system.h"

#define REG(address) (*(volatile uint32_t*)(address))

enum { LOAD = 1, STORE = 2, SHIFT_WEIGHTS = 3, MATMUL = 4, ACTIVATE = 5 };
#define TO_ACTIVATIONS 0u
#define TO_WEIGHTS (1u << 30)
#define TO_BIAS (2u << 30)

static int8_t requantize(int32_t acc, uint32_t scale, uint32_t shift, int relu) {
    int64_t v = (int64_t)acc * (int64_t)scale;
    if (shift) v += (int64_t)1 << (shift - 1);
    v >>= shift;
    if (v > 127) v = 127;
    if (v < -128) v = -128;
    if (relu && v < 0) v = 0;
    return (int8_t)v;
}

void cpu_layer(const int8_t* x, uint32_t rows, uint32_t inputs, uint32_t outputs, const int8_t* w,
               const int32_t* bias, int8_t* out, uint32_t scale, uint32_t shift, int relu) {
    for (uint32_t m = 0; m < rows; ++m)
        for (uint32_t o = 0; o < outputs; ++o) {
            uint32_t acc = (uint32_t)bias[o];
            const int8_t* xr = x + m * inputs;
            const int8_t* wc = w + o;
            for (uint32_t k = 0; k < inputs; ++k) acc += (uint32_t)((int32_t)xr[k] * (int32_t)wc[k * outputs]);
            out[m * outputs + o] = requantize((int32_t)acc, scale, shift, relu);
        }
}

void cpu_conv(const int8_t* image, uint32_t height, uint32_t width, uint32_t channels, uint32_t filters,
              uint32_t stride, const int8_t* w, const int32_t* bias, int8_t* out,
              uint32_t scale, uint32_t shift, int relu) {
    const uint32_t oh = (height - 3) / stride + 1, ow = (width - 3) / stride + 1;
    for (uint32_t oy = 0; oy < oh; ++oy)
        for (uint32_t ox = 0; ox < ow; ++ox)
            for (uint32_t f = 0; f < filters; ++f) {
                uint32_t acc = (uint32_t)bias[f];
                uint32_t k = 0;
                for (uint32_t ky = 0; ky < 3; ++ky) {
                    const int8_t* row = image + ((oy * stride + ky) * width + ox * stride) * channels;
                    for (uint32_t i = 0; i < 3 * channels; ++i, ++k)
                        acc += (uint32_t)((int32_t)row[i] * (int32_t)w[k * filters + f]);
                }
                out[(oy * ow + ox) * filters + f] = requantize((int32_t)acc, scale, shift, relu);
            }
}

static uint32_t queued;
static uint32_t size_n, act_rows, acc_rows;

uint32_t matrix_size(void) {
    if (!size_n) {
        size_n = REG(MATRIX_SIZE);
        act_rows = REG(MATRIX_ACT_ROWS);
        acc_rows = REG(MATRIX_ACC_ROWS);
        queued = REG(MATRIX_DONE);
    }
    return size_n;
}
uint32_t matrix_commands(void) { return queued; }

static void command(uint32_t op, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    REG(MATRIX_ARG0) = a0;
    REG(MATRIX_ARG1) = a1;
    REG(MATRIX_ARG2) = a2;
    REG(MATRIX_ARG3) = a3;
    REG(MATRIX_COMMAND) = op;     /* waits at the port while the queue is full */
    ++queued;
}

int matrix_wait(void) {
    while (REG(MATRIX_DONE) != queued) {}
    __asm__ volatile("fence" ::: "memory");
    return (REG(MATRIX_STATUS) & MATRIX_STATUS_ERROR) != 0;
}

uint32_t matrix_tile_bytes(uint32_t inputs, uint32_t outputs) {
    const uint32_t n = matrix_size();
    return ((inputs + n - 1) / n) * ((outputs + n - 1) / n) * n * n;
}

void matrix_tile(const int8_t* w, uint32_t inputs, uint32_t outputs, int8_t* tiles) {
    const uint32_t n = matrix_size(), chunks = (inputs + n - 1) / n, count = (outputs + n - 1) / n;
    for (uint32_t t = 0; t < count; ++t)
        for (uint32_t c = 0; c < chunks; ++c)
            for (uint32_t r = 0; r < n; ++r)
                for (uint32_t j = 0; j < n; ++j) {
                    const uint32_t k = c * n + r, o = t * n + j;
                    *tiles++ = k < inputs && o < outputs ? w[k * outputs + o] : 0;
                }
}

/* Rows 0 .. count·chunks hold `count` gathered patches of `chunks` rows each;
 * for each output tile: bias, then per chunk the weights, shift and multiply,
 * then requantize into the rows after the patches and store them. */
static void compute_group(uint32_t chunks, uint32_t outputs, const int8_t* tiles, const int32_t* bias,
                          uint32_t count, int8_t* out, uint32_t out_stride, uint32_t scale, uint32_t shift, int relu) {
    const uint32_t n = size_n, tile_count = (outputs + n - 1) / n, out_row = count * chunks;
    for (uint32_t t = 0; t < tile_count; ++t) {
        const uint32_t columns = outputs - t * n < n ? outputs - t * n : n;
        command(LOAD, (uint32_t)(bias + t * n), 1u << 16, (1u << 24) | (4 * n) << 16, TO_BIAS);
        for (uint32_t c = 0; c < chunks; ++c) {
            command(LOAD, (uint32_t)(tiles + (t * chunks + c) * n * n), 1u << 16, n << 24 | n << 16 | n, TO_WEIGHTS);
            command(SHIFT_WEIGHTS, 0, 0, 0, 0);
            command(MATMUL, count << 16 | c, chunks, c ? 1u : 0u, 0);
        }
        command(ACTIVATE, count << 16, out_row, (relu ? 1u << 24 : 0u) | shift << 16 | scale, 0);
        command(STORE, (uint32_t)(out + t * n), count << 16 | out_row, 1u << 24 | columns << 16, 1u << 16 | out_stride);
    }
}

static uint32_t group_size(uint32_t chunks) {
    uint32_t g = act_rows / (chunks + 1);
    if (g > acc_rows) g = acc_rows;
    return g;
}

void matrix_layer(const int8_t* x, uint32_t rows, uint32_t inputs, uint32_t outputs, const int8_t* tiles,
                  const int32_t* bias, int8_t* out, uint32_t scale, uint32_t shift, int relu) {
    const uint32_t n = matrix_size(), chunks = (inputs + n - 1) / n, g = group_size(chunks);
    for (uint32_t first = 0; first < rows; first += g) {
        const uint32_t count = rows - first < g ? rows - first : g;
        /* Each row of x: `chunks` rows of n bytes from consecutive memory. */
        command(LOAD, (uint32_t)(x + first * inputs), count << 16, chunks << 24 | n << 16 | n,
                chunks << 16 | inputs | TO_ACTIVATIONS);
        compute_group(chunks, outputs, tiles, bias, count, out + first * outputs, outputs, scale, shift, relu);
    }
}

void matrix_conv(const int8_t* image, uint32_t height, uint32_t width, uint32_t channels, uint32_t filters,
                 uint32_t stride, const int8_t* tiles, const int32_t* bias, int8_t* out,
                 uint32_t scale, uint32_t shift, int relu) {
    const uint32_t n = matrix_size(), inputs = 9 * channels, chunks = (inputs + n - 1) / n;
    const uint32_t oh = (height - 3) / stride + 1, ow = (width - 3) / stride + 1, patches = oh * ow;
    const uint32_t g = group_size(chunks);
    for (uint32_t first = 0; first < patches; first += g) {
        const uint32_t count = patches - first < g ? patches - first : g;
        for (uint32_t q = first; q < first + count;) {
            const uint32_t oy = q / ow, ox = q % ow;
            uint32_t run = ow - ox;
            if (run > first + count - q) run = first + count - q;
            const int8_t* src = image + ((oy * stride) * width + ox * stride) * channels;
            /* Three rows of 3·channels bytes, the next patch `stride` pixels on. */
            command(LOAD, (uint32_t)src, run << 16 | (q - first) * chunks, 3u << 24 | (3 * channels) << 16 | width * channels,
                    chunks << 16 | stride * channels | TO_ACTIVATIONS);
            q += run;
        }
        compute_group(chunks, filters, tiles, bias, count, out + first * filters, filters, scale, shift, relu);
    }
}
