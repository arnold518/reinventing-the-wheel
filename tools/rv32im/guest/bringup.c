#include "platform.h"
// One executable exercises guest-originated console, sampled timer, queued tap,
// byte lanes, ordered drawing and PRESENT. Report shares the game report page.
int main(void) {
    volatile uint32_t *report=platform_report();
    console_write("hello-device\n");
    report[0]=ticks();report[1]=io_read(TIMER_DIVISOR);report[2]=io_read(TIMER_FREQUENCY_HZ);
#ifdef LEDS_LEDS
    io_write(LEDS_LEDS,0x00ffu);   /* the LED machine: light the low byte */
#endif
    while(!event_count()){} uint32_t e=event_peek();report[3]=e;event_pop();
    clear_screen(0);rect(0,0,4,4,224);pixel(1,1,28);pixel(2,2,3);present();
    rect(0,0,4,4,0);rect((e&31u)*4,4,4,4,255);
#ifdef LEDS_LEDS
    io_write(LEDS_BLINK,2u);       /* then blink it: 4 cycles on, 4 off */
#endif
    present();
    report[4]=io_read(DISPLAY_FRAME_SEQUENCE);report[5]=ticks();io_order();return 0;
}
