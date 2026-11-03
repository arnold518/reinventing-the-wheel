// Test equipment only. Runs the same guest C on the host, with stand-in
// devices, to compare its frames and report with the circuit run;
// independent hard-coded rule tests (RV32IMGameRulesTest) establish the rules.
//
// Short fixtures: REFERENCE_BUTTONS/REFERENCE_DOWN are queued before start.
// Long fixtures (GAME_SESSION 1): session.h holds the computer player's
// batches; one batch is queued after each PRESENT, as on the machine.
#include <stdint.h>
#include "games.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef GAME_KIND
#define GAME_KIND 1
#endif
#ifndef GAME_SESSION
#define GAME_SESSION 0
#endif
#ifndef REFERENCE_WIDTH
#define REFERENCE_WIDTH 64
#endif
#ifndef REFERENCE_HEIGHT
#define REFERENCE_HEIGHT 48
#endif
#if GAME_SESSION
#define SESSION_REFERENCE_DRIVER 1
#include "session.h"
#endif
enum { PIXELS=REFERENCE_WIDTH*REFERENCE_HEIGHT, MAX_FRAMES=1024 };
#if GAME_KIND == 4
const Game *guest_reference_game_state(void);
static void check_pong_pixels(const uint8_t *pixels){
 const Game *g=guest_reference_game_state();const uint8_t palette[4]={0,224,28,3};
 const unsigned cw=REFERENCE_WIDTH/32,ch=REFERENCE_HEIGHT/16;
 // Independent full-grid projection checks the incremental sprite renderer.
 for(unsigned y=0;y<16*ch;++y)for(unsigned x=0;x<32*cw;++x)
  if(pixels[y*REFERENCE_WIDTH+x]!=palette[game_cell(g,x/cw,y/ch)]){fputs("Pong sprite/grid pixels differ\n",stderr);exit(1);}
}
#endif
static uint32_t reports[9],events[8],count,head,levels,sequence;
#if GAME_SESSION
static uint32_t batch;
#endif
static uint8_t pixels[PIXELS],frames[MAX_FRAMES][PIXELS];
static uint32_t frame_levels[MAX_FRAMES];
static char console[256];static unsigned console_size;
static void fail(const char *why){fputs(why,stderr);fputc('\n',stderr);exit(1);}
volatile uint32_t *reference_report(void){return reports;}
uint32_t reference_read(uint32_t address){
 switch(address){
 case 0x10000004:return 1;case 0x10001000:return 123;case 0x10001004:return 1;case 0x10001008:return 0;
 case 0x10003000:return REFERENCE_WIDTH;case 0x10003004:return REFERENCE_HEIGHT;case 0x10003008:return REFERENCE_WIDTH;case 0x1000300c:return 1;
 case 0x10002000:return levels;case 0x10002004:if(!count)fail("empty event read");return events[head%8];
 case 0x10002008:return count;case 0x1000200c:return count!=0;case 0x10003010:return sequence;
 default:fprintf(stderr,"unexpected reference read %x\n",address);exit(1);
 }
}
void reference_write8(uint32_t address,uint8_t value){
 if(address<0x20000000||address>=0x20000000+PIXELS)fail("bad pixel write");
 pixels[address-0x20000000]=value;
}
static void event(unsigned button,unsigned down){
 if(count>=8)fail("reference queue overflow");
 events[(head+count++)%8]=button|(down<<8);levels=(levels&~(1u<<button))|(down<<button);
}
void reference_write(uint32_t address,uint32_t value){
 if(address>=0x20000000&&address<0x20000000+PIXELS){for(unsigned i=0;i<4;++i)reference_write8(address+i,value>>(8*i));return;}
 switch(address){
 case 0x10000000:if(console_size>=sizeof(console)-1)exit(1);console[console_size++]=value;break;
 case 0x10002010:if(value!=1||!count)exit(1);++head;--count;break;
 case 0x10004000:case 0x10004004:break;   // LED panel: not part of the frames
 case 0x10003014:if(value!=1||sequence>=MAX_FRAMES)exit(1);
#if GAME_KIND == 4
  check_pong_pixels(pixels);
#endif
  frame_levels[sequence]=levels;memcpy(frames[sequence++],pixels,PIXELS);
#if GAME_SESSION
  if(count)fail("a batch was left in the queue at PRESENT");
  if(batch<SESSION_STEPS){
   for(unsigned i=0;i<session_batch_sizes[batch];++i)event(session_batches[batch][i][0],session_batches[batch][i][1]);
   ++batch;
  }
#endif
  break;
 default:fprintf(stderr,"unexpected reference write %x\n",address);exit(1);
 }
}
int guest_main(void);
static void hex(const uint8_t *p,unsigned n){for(unsigned i=0;i<n;++i)printf("%02x",p[i]);}
int main(void){
#if !GAME_SESSION
 const unsigned buttons[]={REFERENCE_BUTTONS},down[]={REFERENCE_DOWN};
 for(unsigned i=0;i<sizeof(buttons)/sizeof(buttons[0]);++i)event(buttons[i],down[i]);
#endif
 if(guest_main()!=0)return 1;
#if GAME_SESSION
 if(batch!=SESSION_STEPS||count)fail("the session did not take every batch");
#endif
 console[console_size]=0;
 printf("{\"game\":%u,\"report\":[",GAME_KIND);for(unsigned i=0;i<9;++i)printf("%s%u",i?",":"",reports[i]);
 printf("],\"console\":\"");for(unsigned i=0;i<console_size;++i){if(console[i]=='\n')fputs("\\n",stdout);else putchar(console[i]);}
 printf("\",\"levels\":[");for(unsigned i=0;i<sequence;++i)printf("%s%u",i?",":"",frame_levels[i]);
 printf("],\"frames\":[");for(unsigned i=0;i<sequence;++i){printf("%s\"",i?",":"");hex(frames[i],PIXELS);putchar('"');}puts("]}");return 0;
}
