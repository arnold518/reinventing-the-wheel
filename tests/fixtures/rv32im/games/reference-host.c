// Test equipment only. Runs the same guest C on the host to compare compiled
// game reports/drawing; independent hard-coded rule tests establish rules.
#include <stdint.h>
#include "games.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef GAME_KIND
#define GAME_KIND 1
#endif
#if GAME_KIND == 4
const Game *guest_reference_game_state(void);
static void check_pong_pixels(const uint8_t *pixels){
 const Game *g=guest_reference_game_state();const uint8_t palette[4]={0,224,28,3};
 // Independent full-grid projection checks the incremental sprite renderer.
 for(unsigned y=0;y<48;++y)for(unsigned x=0;x<64;++x)
  if(pixels[y*64+x]!=palette[game_cell(g,x/2,y/3)]){fputs("Pong sprite/grid pixels differ\n",stderr);exit(1);}
}
#endif
static uint32_t reports[9],events[4],count,head,levels,sequence;
static uint8_t pixels[4096],frames[64][3072];
static char console[256];static unsigned console_size;
volatile uint32_t *reference_report(void){return reports;}
uint32_t reference_read(uint32_t address){
 switch(address){
 case 0x10000004:return 1;case 0x10001000:return 123;case 0x10001004:return 1;case 0x10001008:return 0;
 case 0x10002000:return levels;case 0x10002004:if(!count){fputs("empty event read\n",stderr);exit(1);}return events[head];
 case 0x10002008:return count;case 0x1000200c:return count!=0;case 0x10003010:return sequence;
 default:fprintf(stderr,"unexpected reference read %x\n",address);exit(1);
 }
}
void reference_write8(uint32_t address,uint8_t value){
 if(address<0x20000000||address>=0x20001000){fputs("bad pixel write\n",stderr);exit(1);}pixels[address-0x20000000]=value;
}
void reference_write(uint32_t address,uint32_t value){
 if(address>=0x20000000&&address<0x20001000){for(unsigned i=0;i<4;++i)reference_write8(address+i,value>>(8*i));return;}
 switch(address){
 case 0x10000000:if(console_size>=sizeof(console)-1)exit(1);console[console_size++]=value;break;
 case 0x10002010:if(value!=1||!count)exit(1);++head;--count;break;
 case 0x10003014:if(value!=1||sequence>=64)exit(1);
#if GAME_KIND == 4
  check_pong_pixels(pixels);
#endif
  memcpy(frames[sequence++],pixels,3072);break;
 default:fprintf(stderr,"unexpected reference write %x\n",address);exit(1);
 }
}
int guest_main(void);
static void event(unsigned button,unsigned down){events[count++]=button|(down<<8);levels=(levels&~(1u<<button))|(down<<button);}
static void hex(const uint8_t *p,unsigned count){for(unsigned i=0;i<count;++i)printf("%02x",p[i]);}
int main(void){
 const unsigned buttons[]={REFERENCE_BUTTONS},down[]={REFERENCE_DOWN};
 for(unsigned i=0;i<sizeof(buttons)/sizeof(buttons[0]);++i)event(buttons[i],down[i]);
 if(guest_main()!=0)return 1;
 printf("{\"game\":%u,\"report\":[",GAME_KIND);for(unsigned i=0;i<9;++i)printf("%s%u",i?",":"",reports[i]);
 printf("],\"frames\":[");for(unsigned i=0;i<sequence;++i){printf("%s\"",i?",":"");hex(frames[i],3072);putchar('"');}puts("]}");return 0;
}
