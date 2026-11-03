/* Generated from systems/games-machine-leds.json by system_header; do not edit. */
#ifndef CIRCUITSIM_SYSTEM_GAMES_MACHINE_LEDS_H
#define CIRCUITSIM_SYSTEM_GAMES_MACHINE_LEDS_H

/* Address map: what the CPU sees. */
#define RAM_BASE 0x00000000u
#define RAM_BYTES 0x00040000u
#define CONSOLE_BASE 0x10000000u
#define CONSOLE_BYTES 0x00000100u
#define TIMER_BASE 0x10001000u
#define TIMER_BYTES 0x00000100u
#define BUTTONS_BASE 0x10002000u
#define BUTTONS_BYTES 0x00000100u
#define DISPLAY_BASE 0x10003000u
#define DISPLAY_BYTES 0x00000100u
#define LEDS_BASE 0x10004000u
#define LEDS_BYTES 0x00000100u
#define VIDEO_BASE 0x20000000u
#define VIDEO_BYTES 0x00001000u

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

/* BUTTONS (device.buttons) registers */
#define BUTTONS_LEVELS 0x10002000u /* read-only: Bit n: button n is held */
#define BUTTONS_LEVELS_OFFSET 0x000u
#define BUTTONS_HEAD 0x10002004u /* read-only: Oldest event: button in bits 4..0, bit 8 set for a press */
#define BUTTONS_HEAD_OFFSET 0x004u
#define BUTTONS_HEAD_BUTTON_SHIFT 0u
#define BUTTONS_HEAD_BUTTON_MASK 0x1fu
#define BUTTONS_HEAD_DOWN (1u << 8)
#define BUTTONS_COUNT 0x10002008u /* read-only: Events waiting */
#define BUTTONS_COUNT_OFFSET 0x008u
#define BUTTONS_STATUS 0x1000200cu /* read-only: Bit 0: an event waits; bit 1: an event was lost */
#define BUTTONS_STATUS_OFFSET 0x00cu
#define BUTTONS_STATUS_VALID (1u << 0)
#define BUTTONS_STATUS_OVERFLOW (1u << 1)
#define BUTTONS_POP 0x10002010u /* command: Remove the oldest event */
#define BUTTONS_POP_OFFSET 0x010u
#define BUTTONS_CLEAR_OVERFLOW 0x10002014u /* command: Clear the lost-event flag */
#define BUTTONS_CLEAR_OVERFLOW_OFFSET 0x014u

/* DISPLAY (device.framebuffer) registers */
#define DISPLAY_WIDTH 0x10003000u /* setting: Visible pixels per row */
#define DISPLAY_WIDTH_OFFSET 0x000u
#define DISPLAY_HEIGHT 0x10003004u /* setting: Visible rows */
#define DISPLAY_HEIGHT_OFFSET 0x004u
#define DISPLAY_STRIDE 0x10003008u /* setting: Bytes from one row to the next in video memory */
#define DISPLAY_STRIDE_OFFSET 0x008u
#define DISPLAY_FORMAT 0x1000300cu /* setting: Pixel format; 1 is RGB332 (one byte per pixel) */
#define DISPLAY_FORMAT_OFFSET 0x00cu
#define DISPLAY_FRAME_SEQUENCE 0x10003010u /* read-only: Frames presented since reset */
#define DISPLAY_FRAME_SEQUENCE_OFFSET 0x010u
#define DISPLAY_PRESENT 0x10003014u /* command: Show the current video memory as a new frame */
#define DISPLAY_PRESENT_OFFSET 0x014u

/* LEDS (device.leds) registers */
#define LEDS_LEDS 0x10004000u /* read-write: Bit n: LED n is on */
#define LEDS_LEDS_OFFSET 0x000u
#define LEDS_BLINK 0x10004004u /* read-write: 0: steady; k: lit LEDs blink, on and off for 2^k cycles each */
#define LEDS_BLINK_OFFSET 0x004u

#endif
