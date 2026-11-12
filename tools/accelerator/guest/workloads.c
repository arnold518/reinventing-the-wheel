/* Chapter 6 workloads: one program per workload and machine, chosen at build time.
 *   -DWORKLOAD=1  matrix multiply (dense layers of three sizes, random operands)
 *   -DWORKLOAD=2  MNIST MLP 784 -> 64 -> 10
 *   -DWORKLOAD=3  MNIST CNN (3x3/8/stride 2 -> 3x3/16/stride 2 -> 576 -> 10)
 *   -DMATRIX_N=n  use MatrixUnit<n>; without it the CPU computes everything.
 * Results and timer readings go to the report page at 0x3f000 (layout below);
 * the host compares the outputs with its own integer reference.
 */
#include <stdint.h>
#include "system.h"
#include "matrix.h"
#include "images.h"
#ifdef MATRIX_N
#define STR2(x) #x
#define STR(x) STR2(x)
#if MATRIX_N == 4
#include "model_tiled_n4.h"
#elif MATRIX_N == 8
#include "model_tiled_n8.h"
#else
#include "model_tiled_n16.h"
#endif
#else
#include "model_cpu.h"
#endif

#define REG(address) (*(volatile uint32_t*)(address))
#ifndef IMAGES_RUN
#define IMAGES_RUN IMAGE_COUNT
#endif

/* Report page: words 0..63, then output bytes from 0x100. */
struct report {
    uint32_t magic, workload, n, status, sections, commands;
    uint32_t cycles[58];
};
#define REPORT ((volatile struct report*)0x3f000)
#define OUTPUTS ((volatile int8_t*)0x3f100)

static uint32_t now(void) { return REG(TIMER_COUNT); }
static uint32_t section_start;
static void begin(void) { section_start = now(); }
static void end(void) {
    const uint32_t s = REPORT->sections;
    REPORT->cycles[s] = now() - section_start;
    REPORT->sections = s + 1;
}
static __attribute__((unused)) void fail(uint32_t code) { if (!REPORT->status) REPORT->status = code; }
static void put(const char* s) { while (*s) REG(CONSOLE_TX) = (uint8_t)*s++; }

#ifdef MATRIX_N
static void finish(void) { if (matrix_wait()) fail(2); }
#else
static void finish(void) {}
#endif

#if WORKLOAD == 1 || WORKLOAD == 4
static uint32_t seed = 0x2545f491u;
static int8_t random8(void) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (int8_t)(seed >> 8); }
#endif

#if WORKLOAD == 1
/* Dense layers (rows × inputs → outputs) with xorshift operands; scale 1, shift 8. */
static const uint32_t shapes[3][3] = {{4, 32, 16}, {8, 64, 32}, {16, 64, 64}};
static int8_t x[16 * 64 + 64], w[64 * 64], tiled[64 * 64 + 16 * 16 * 4], out[16 * 64];
static int32_t bias[64 + 16];
static void run(void) {
    volatile int8_t* o = OUTPUTS;
    for (int s = 0; s < 3; ++s) {
        const uint32_t rows = shapes[s][0], inputs = shapes[s][1], outputs = shapes[s][2];
        for (uint32_t i = 0; i < rows * inputs; ++i) x[i] = random8();
        for (uint32_t i = 0; i < inputs * outputs; ++i) w[i] = random8();
        for (uint32_t i = 0; i < outputs; ++i) bias[i] = (int32_t)random8() * 37;
#ifdef MATRIX_N
        matrix_tile(w, inputs, outputs, tiled);      /* weight layout: preparation, not timed */
        begin();
        matrix_layer(x, rows, inputs, outputs, tiled, bias, out, 1, 8, 0);
        finish();
#else
        begin();
        cpu_layer(x, rows, inputs, outputs, w, bias, out, 1, 8, 0);
#endif
        end();
        for (uint32_t i = 0; i < rows * outputs; ++i) *o++ = out[i];
    }
}
#elif WORKLOAD == 2
static int8_t hidden[IMAGE_COUNT * 64], logits[IMAGE_COUNT * 10];
static void run(void) {
    for (uint32_t i = 0; i < IMAGES_RUN; ++i) {
        const int8_t* image = images + i * 784;
        begin();
#ifdef MATRIX_N
        matrix_layer(image, 1, 784, 64, mlp_l1_tiles, mlp_l1_bias, hidden, MLP_SCALE1, MLP_SHIFT1, 1);
        matrix_layer(hidden, 1, 64, 10, mlp_l2_tiles, mlp_l2_bias, logits + i * 10, MLP_SCALE2, MLP_SHIFT2, 0);
        finish();
#else
        cpu_layer(image, 1, 784, 64, mlp_w1, mlp_b1, hidden, MLP_SCALE1, MLP_SHIFT1, 1);
        cpu_layer(hidden, 1, 64, 10, mlp_w2, mlp_b2, logits + i * 10, MLP_SCALE2, MLP_SHIFT2, 0);
#endif
        end();
    }
#ifdef MATRIX_N
    /* The same images as one batch: each weight tile is loaded once for all of them. */
    static int8_t batch[IMAGE_COUNT * 10];
    begin();
    matrix_layer(images, IMAGES_RUN, 784, 64, mlp_l1_tiles, mlp_l1_bias, hidden, MLP_SCALE1, MLP_SHIFT1, 1);
    matrix_layer(hidden, IMAGES_RUN, 64, 10, mlp_l2_tiles, mlp_l2_bias, batch, MLP_SCALE2, MLP_SHIFT2, 0);
    finish();
    end();
    for (uint32_t i = 0; i < IMAGES_RUN * 10; ++i) if (batch[i] != logits[i]) fail(3);
#endif
    for (uint32_t i = 0; i < IMAGES_RUN * 10; ++i) OUTPUTS[i] = logits[i];
}
#elif WORKLOAD == 4
/* The machine scenario: one 4 × 16 → 8 layer on the CPU and (with a unit) on the unit, compared. */
static int8_t x[4 * 16 + 16], w[16 * 8], tiled[16 * 8 * 4], out_cpu[4 * 8], out_unit[4 * 8];
static int32_t bias[8 + 16];
static void run(void) {
    for (uint32_t i = 0; i < sizeof x - 16; ++i) x[i] = random8();
    for (uint32_t i = 0; i < sizeof w; ++i) w[i] = random8();
    for (uint32_t i = 0; i < 8; ++i) bias[i] = (int32_t)random8() * 11;
    begin();
    cpu_layer(x, 4, 16, 8, w, bias, out_cpu, 1, 6, 1);
    end();
#ifdef MATRIX_N
    matrix_tile(w, 16, 8, tiled);
    begin();
    matrix_layer(x, 4, 16, 8, tiled, bias, out_unit, 1, 6, 1);
    finish();
    end();
    for (uint32_t i = 0; i < sizeof out_cpu; ++i) if (out_unit[i] != out_cpu[i]) fail(4);
#endif
}
#else
static int8_t a1[13 * 13 * 8], a2[6 * 6 * 16], logits[IMAGE_COUNT * 10];
static void run(void) {
    for (uint32_t i = 0; i < IMAGES_RUN; ++i) {
        const int8_t* image = images + i * 784;
        begin();
#ifdef MATRIX_N
        matrix_conv(image, 28, 28, 1, 8, 2, cnn_l1_tiles, cnn_l1_bias, a1, CNN_SCALE1, CNN_SHIFT1, 1);
        matrix_conv(a1, 13, 13, 8, 16, 2, cnn_l2_tiles, cnn_l2_bias, a2, CNN_SCALE2, CNN_SHIFT2, 1);
        matrix_layer(a2, 1, 576, 10, cnn_l3_tiles, cnn_l3_bias, logits + i * 10, CNN_SCALE3, CNN_SHIFT3, 0);
        finish();
#else
        cpu_conv(image, 28, 28, 1, 8, 2, cnn_c1, cnn_d1, a1, CNN_SCALE1, CNN_SHIFT1, 1);
        cpu_conv(a1, 13, 13, 8, 16, 2, cnn_c2, cnn_d2, a2, CNN_SCALE2, CNN_SHIFT2, 1);
        cpu_layer(a2, 1, 576, 10, cnn_f, cnn_e, logits + i * 10, CNN_SCALE3, CNN_SHIFT3, 0);
#endif
        end();
    }
    for (uint32_t i = 0; i < IMAGES_RUN * 10; ++i) OUTPUTS[i] = logits[i];
}
#endif

int main(void) {
    REPORT->magic = 0x31434341u;   /* "ACC1" */
    REPORT->workload = WORKLOAD;
    REPORT->status = 0;
    REPORT->sections = 0;
#ifdef MATRIX_N
    REPORT->n = MATRIX_N;
    if (matrix_size() != MATRIX_N) { fail(1); put("wrong unit size\n"); return 0; }
#else
    REPORT->n = 0;
#endif
    run();
#ifdef MATRIX_N
    REPORT->commands = matrix_commands();
#endif
    put(REPORT->status ? "fail\n" : "done\n");
    return 0;
}
