#include "platform.h"
#include <stddef.h>
void console_write(const char *s) {
    while (*s) { while (!io_read(CONSOLE_STATUS)) {} io_write(CONSOLE_TX,(uint8_t)*s++); }
}
static uint32_t surface_width, surface_height, surface_stride;
static int surface_initialized;
static void display_init(void) {
    if (surface_initialized) return;
    surface_initialized=1;
    uint32_t width=io_read(DISPLAY_WIDTH), height=io_read(DISPLAY_HEIGHT);
    uint32_t stride=io_read(DISPLAY_STRIDE), format=io_read(DISPLAY_FORMAT);
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
        io_write8(VIDEO_BASE+(unsigned)y*surface_stride+(unsigned)x,color);
}
void rect(int x, int y, int width, int height, uint8_t color) {
    display_init();
    if (width<=0 || height<=0 || !surface_width) return;
    uint32_t first, top, columns, rows;
    if ((uint32_t)x<surface_width && (uint32_t)y<surface_height &&
        (uint32_t)width<=surface_width-(uint32_t)x && (uint32_t)height<=surface_height-(uint32_t)y) {
        first=(uint32_t)x;top=(uint32_t)y;columns=(uint32_t)width;rows=(uint32_t)height;   // on screen
    } else {
        // Clip before adding coordinates (negative or off-screen origins and
        // very large extents included).
        int64_t left=x<0?0:x, upper=y<0?0:y;
        int64_t right=(int64_t)x+width, bottom=(int64_t)y+height;
        if (right>(int64_t)surface_width) right=surface_width;
        if (bottom>(int64_t)surface_height) bottom=surface_height;
        if (left>=right || upper>=bottom) return;
        first=(uint32_t)left;top=(uint32_t)upper;columns=(uint32_t)(right-left);rows=(uint32_t)(bottom-upper);
    }
    uint32_t packed=(uint32_t)color*0x01010101u;
    uint32_t row_address=VIDEO_BASE+top*surface_stride+first;
    for (; rows; --rows, row_address+=surface_stride) {
        uint32_t address=row_address, end=row_address+columns;
        // Alignment includes the row stride, which need not be divisible by four.
        while (address<end && (address&3u)) io_write8(address++,color);
        for (; address+4<=end; address+=4) io_write(address,packed);
        while (address<end) io_write8(address++,color);
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
