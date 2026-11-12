/* Driver for the chapter 6 matrix unit (MatrixUnit<N>), and the CPU versions
 * of the same integer layers. Both compute, bit for bit:
 *   out = clamp((bias + sum x*w) * scale + 2^(shift-1)) >> shift, -128, 127), then ReLU,
 * with int32 sums. Weights for the unit are pre-tiled by tools/accelerator/model.py
 * (or matrix_tile below): [output tile][input chunk][row][column], N × N bytes each.
 */
#ifndef ACCELERATOR_MATRIX_H
#define ACCELERATOR_MATRIX_H
#include <stdint.h>

/* CPU layers: x is rows × inputs, w is inputs × outputs (row-major). */
void cpu_layer(const int8_t* x, uint32_t rows, uint32_t inputs, uint32_t outputs, const int8_t* w,
               const int32_t* bias, int8_t* out, uint32_t scale, uint32_t shift, int relu);
/* 3×3 convolution, no padding: HWC image, w is (9·channels) × filters with
 * rows in (ky, kx, channel) order; out is HWC. */
void cpu_conv(const int8_t* image, uint32_t height, uint32_t width, uint32_t channels, uint32_t filters,
              uint32_t stride, const int8_t* w, const int32_t* bias, int8_t* out,
              uint32_t scale, uint32_t shift, int relu);

/* Unit layers: the same results, computed by MatrixUnit<n> (n = its SIZE). */
uint32_t matrix_size(void);
void matrix_layer(const int8_t* x, uint32_t rows, uint32_t inputs, uint32_t outputs, const int8_t* tiles,
                  const int32_t* bias, int8_t* out, uint32_t scale, uint32_t shift, int relu);
void matrix_conv(const int8_t* image, uint32_t height, uint32_t width, uint32_t channels, uint32_t filters,
                 uint32_t stride, const int8_t* tiles, const int32_t* bias, int8_t* out,
                 uint32_t scale, uint32_t shift, int relu);
/* Waits for every queued command; returns nonzero when the unit reported an error. */
int matrix_wait(void);
/* Writes w (inputs × outputs) in the unit's tile order into tiles (sized by matrix_tile_bytes). */
uint32_t matrix_tile_bytes(uint32_t inputs, uint32_t outputs);
void matrix_tile(const int8_t* w, uint32_t inputs, uint32_t outputs, int8_t* tiles);
uint32_t matrix_commands(void);

#endif
