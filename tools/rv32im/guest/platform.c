#include "platform.h"
#include <stddef.h>
void console_write(const char *s) {
    while (*s) { while (!io_read(0x10000004u)) {} io_write(0x10000000u,(uint8_t)*s++); }
}
void pixel(int x, int y, uint8_t color) {
    if ((unsigned)x<SCREEN_W && (unsigned)y<SCREEN_H)
        io_write8(0x20000000u+(unsigned)y*SCREEN_W+(unsigned)x,color);
}
void rect(int x, int y, int width, int height, uint8_t color) {
    // Aligned spans use ordinary word stores; edges keep byte-lane semantics.
    for (int row=0; row<height; ++row) {
        int yy=y+row; if ((unsigned)yy>=SCREEN_H) continue;
        int left=x<0?0:x, right=x+width; if (right>(int)SCREEN_W) right=SCREEN_W;
        while (left<right && (left&3)) pixel(left++,yy,color);
        uint32_t packed=(uint32_t)color*0x01010101u;
        while (left+4<=right) { io_write(0x20000000u+(unsigned)yy*SCREEN_W+(unsigned)left,packed); left+=4; }
        while (left<right) pixel(left++,yy,color);
    }
}
void clear_screen(uint8_t color) { rect(0,0,SCREEN_W,SCREEN_H,color); }
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
