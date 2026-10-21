#include "platform.h"
// One executable exercises guest-originated console, sampled timer, queued tap,
// byte lanes, ordered drawing and PRESENT. Report shares the game report page.
int main(void) {
    volatile uint32_t *report=platform_report();
    console_write("hello-device\n");
    report[0]=ticks();report[1]=io_read(0x10001004u);report[2]=io_read(0x10001008u);
    while(!event_count()){} uint32_t e=event_peek();report[3]=e;event_pop();
    clear_screen(0);rect(0,0,4,4,224);pixel(1,1,28);pixel(2,2,3);present();
    rect(0,0,4,4,0);rect((e&31u)*4,4,4,4,255);present();
    report[4]=io_read(0x10003010u);report[5]=ticks();io_order();return 0;
}
