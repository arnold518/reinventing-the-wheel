/* Generated from systems/accelerator-machine.json by system_header; do not edit. */
#ifndef CIRCUITSIM_SYSTEM_ACCELERATOR_MACHINE_H
#define CIRCUITSIM_SYSTEM_ACCELERATOR_MACHINE_H

/* Address map: what the CPU sees. */
#define RAM_BASE 0x00000000u
#define RAM_BYTES 0x00040000u
#define CONSOLE_BASE 0x10000000u
#define CONSOLE_BYTES 0x00000100u
#define TIMER_BASE 0x10001000u
#define TIMER_BYTES 0x00000100u
#define MATRIX_BASE 0x10005000u
#define MATRIX_BYTES 0x00000100u

/* CONSOLE (device.console) registers */
#define CONSOLE_TX 0x10000000u /* write-only data: Queue one byte for the host (bits 7..0) */
#define CONSOLE_TX_OFFSET 0x000u
#define CONSOLE_STATUS 0x10000004u /* read-only: Bit 0: the queue has room */
#define CONSOLE_STATUS_OFFSET 0x004u
#define CONSOLE_STATUS_ROOM (1u << 0)
#define CONSOLE_COUNT 0x10000008u /* read-only: Bytes waiting in the queue */
#define CONSOLE_COUNT_OFFSET 0x008u

/* TIMER (device.timer) registers */
#define TIMER_COUNT 0x10001000u /* read-only: Ticks since reset */
#define TIMER_COUNT_OFFSET 0x000u
#define TIMER_DIVISOR 0x10001004u /* setting: Clock cycles per tick */
#define TIMER_DIVISOR_OFFSET 0x004u
#define TIMER_FREQUENCY_HZ 0x10001008u /* setting: Declared clock frequency (0: not declared) */
#define TIMER_FREQUENCY_HZ_OFFSET 0x008u

/* MATRIX (device.matrix-unit.16) registers */
#define MATRIX_COMMAND 0x10005000u /* write-only data: Queue a command (1 LOAD, 2 STORE, 3 SHIFT_WEIGHTS, 4 MATMUL, 5 ACTIVATE) with ARG0..ARG3 */
#define MATRIX_COMMAND_OFFSET 0x000u
#define MATRIX_ARG0 0x10005004u /* read-write: First argument of the next command */
#define MATRIX_ARG0_OFFSET 0x004u
#define MATRIX_ARG1 0x10005008u /* read-write: Second argument of the next command */
#define MATRIX_ARG1_OFFSET 0x008u
#define MATRIX_ARG2 0x1000500cu /* read-write: Third argument of the next command */
#define MATRIX_ARG2_OFFSET 0x00cu
#define MATRIX_ARG3 0x10005010u /* read-write: Fourth argument of the next command */
#define MATRIX_ARG3_OFFSET 0x010u
#define MATRIX_STATUS 0x10005014u /* read-only: Bit 0: busy; bit 1: a command failed; bit 2: the queue is full */
#define MATRIX_STATUS_OFFSET 0x014u
#define MATRIX_STATUS_BUSY (1u << 0)
#define MATRIX_STATUS_ERROR (1u << 1)
#define MATRIX_STATUS_FULL (1u << 2)
#define MATRIX_DONE 0x10005018u /* read-only: Commands finished since reset */
#define MATRIX_DONE_OFFSET 0x018u
#define MATRIX_CLEAR 0x1000501cu /* command: Clear the error flag */
#define MATRIX_CLEAR_OFFSET 0x01cu
#define MATRIX_SIZE 0x10005020u /* setting: Array rows and columns (N) */
#define MATRIX_SIZE_OFFSET 0x020u
#define MATRIX_ACT_ROWS 0x10005024u /* setting: Rows of the activation buffer (N bytes each) */
#define MATRIX_ACT_ROWS_OFFSET 0x024u
#define MATRIX_ACC_ROWS 0x10005028u /* setting: Rows of the accumulators (N int32 each) */
#define MATRIX_ACC_ROWS_OFFSET 0x028u

#endif
