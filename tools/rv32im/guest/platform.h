#ifndef CIRCUITSIM_GAME_PLATFORM_H
#define CIRCUITSIM_GAME_PLATFORM_H
#include <stdint.h>
/* Addresses and register offsets, generated from the system description. */
#ifndef SYSTEM_HEADER
#define SYSTEM_HEADER "system.h"
#endif
#include SYSTEM_HEADER
enum { BUTTON_UP, BUTTON_RIGHT, BUTTON_DOWN, BUTTON_LEFT, BUTTON_ACTION,
       BUTTON_RESET, BUTTON_TICK, BUTTON_P2_UP, BUTTON_P2_DOWN, BUTTON_PAUSE };
#ifdef CIRCUITSIM_GUEST_REFERENCE
// Test equipment boundary; never enabled in the audited RV32IM binaries.
uint32_t reference_read(uint32_t);
void reference_write(uint32_t,uint32_t);
void reference_write8(uint32_t,uint8_t);
volatile uint32_t *reference_report(void);
#define io_read reference_read
#define io_write reference_write
#define io_write8 reference_write8
static inline void io_order(void) { __asm__ volatile ("" ::: "memory"); }
static inline volatile uint32_t *platform_report(void) { return reference_report(); }
#else
static inline uint32_t io_read(uint32_t a) { return *(volatile uint32_t *)(uintptr_t)a; }
static inline void io_write(uint32_t a, uint32_t v) { *(volatile uint32_t *)(uintptr_t)a = v; }
static inline void io_write8(uint32_t a, uint8_t v) { *(volatile uint8_t *)(uintptr_t)a = v; }
static inline void io_order(void) { __asm__ volatile ("fence iorw, iorw" ::: "memory"); }
static inline volatile uint32_t *platform_report(void) { return (volatile uint32_t *)(uintptr_t)0x3f000; }
#endif
static inline uint32_t ticks(void) { return io_read(TIMER_COUNT); }
static inline uint32_t button_levels(void) { return io_read(BUTTONS_LEVELS); }
static inline uint32_t event_count(void) { return io_read(BUTTONS_COUNT); }
static inline uint32_t event_peek(void) { return io_read(BUTTONS_HEAD); }
static inline void event_pop(void) { io_write(BUTTONS_POP,1); }
static inline void present(void) { io_order(); io_write(DISPLAY_PRESENT,1); io_order(); }
// Drawing uses the geometry the display reports (DISPLAY_WIDTH, ...).
uint32_t display_width(void);
uint32_t display_height(void);
uint32_t display_stride(void);
void console_write(const char *s);
void pixel(int x, int y, uint8_t color);
void rect(int x, int y, int width, int height, uint8_t color);
void clear_screen(uint8_t color);
uint32_t random_next(uint32_t *state);
#endif
