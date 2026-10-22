#include "platform.h"
#include <stddef.h>
void console_write(const char *s) {
    while (*s) { while (!io_read(0x10000004u)) {} io_write(0x10000000u,(uint8_t)*s++); }
}
static uint32_t surface_width, surface_height, surface_stride;
static int surface_initialized;
static void display_init(void) {
    if (surface_initialized) return;
    surface_initialized=1;
    uint32_t width=io_read(0x10003000u), height=io_read(0x10003004u);
    uint32_t stride=io_read(0x10003008u), format=io_read(0x1000300cu);
    if (format!=1 || !width || !height || width>4096 || height>4096
        || stride<width || stride>262144u/height) return;
    surface_width=width;surface_height=height;surface_stride=stride;
}
uint32_t display_width(void) { display_init();return surface_width; }
uint32_t display_height(void) { display_init();return surface_height; }
uint32_t display_stride(void) { display_init();return surface_stride; }
void pixel(int x, int y, uint8_t color) {
    display_init();
    if ((unsigned)x<surface_width && (unsigned)y<surface_height)
        io_write8(0x20000000u+(unsigned)y*surface_stride+(unsigned)x,color);
}
void rect(int x, int y, int width, int height, uint8_t color) {
    display_init();
    if (width<=0 || height<=0 || !surface_width) return;
    // Clip before adding coordinates, including negative/off-screen origins.
    int left=x<0?0:x, top=y<0?0:y;
    int64_t right=(int64_t)x+width, bottom=(int64_t)y+height;
    if (right>(int64_t)surface_width) right=surface_width;
    if (bottom>(int64_t)surface_height) bottom=surface_height;
    uint32_t packed=(uint32_t)color*0x01010101u;
    for (int row=top; row<bottom; ++row) {
        int col=left;
        uint32_t address=0x20000000u+(unsigned)row*surface_stride+(unsigned)col;
        // Alignment includes the row stride, which need not be divisible by four.
        while (col<right && (address&3u)) { io_write8(address++,color);++col; }
        while ((int64_t)col+4<=right) { io_write(address,packed);address+=4;col+=4; }
        while (col<right) { io_write8(address++,color);++col; }
    }
}
void clear_screen(uint8_t color) { display_init();rect(0,0,surface_width,surface_height,color); }
uint32_t random_next(uint32_t *s) {
    uint32_t x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x;
}

// Compiler-generated freestanding aggregate copies use these ordinary C ABI
// helpers. -fno-builtin prevents a loop from being folded back into itself.
void *memcpy(void *destination, const void *source, size_t count) {
    uint8_t *d=destination; const uint8_t *s=source;
    for(size_t i=0;i<count;++i)d[i]=s[i];
    return destination;
}
void *memset(void *destination, int value, size_t count) {
    uint8_t *d=destination;
    for(size_t i=0;i<count;++i)d[i]=(uint8_t)value;
    return destination;
}
