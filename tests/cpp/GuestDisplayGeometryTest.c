// Host test equipment for the freestanding drawing library, not CPU emulation.
#include "platform.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t width,height,stride,format=1,reads,writes;
static uint8_t pixels[262144];
static void require(int ok,const char *why){if(!ok){fputs(why,stderr);exit(1);}}
uint32_t reference_read(uint32_t address){++reads;switch(address){
 case 0x10003000:return width;case 0x10003004:return height;case 0x10003008:return stride;case 0x1000300c:return format;
 default:require(0,"Unexpected register read");return 0;}}
void reference_write8(uint32_t address,uint8_t value){
 require(address>=0x20000000u && address-0x20000000u<sizeof pixels,"Pixel write outside storage");
 pixels[address-0x20000000u]=value;++writes;
}
void reference_write(uint32_t address,uint32_t value){
 require(!(address&3u),"Unaligned guest word store");
 for(unsigned i=0;i<4;++i)reference_write8(address+i,value>>(8*i));
}
volatile uint32_t *reference_report(void){return NULL;}
int main(int argc,char **argv){
 require(argc==4,"Usage: width height stride");width=strtoul(argv[1],0,10);height=strtoul(argv[2],0,10);stride=strtoul(argv[3],0,10);
 memset(pixels,0xa5,sizeof pixels);clear_screen(0x1c);
 require(display_width()==width && display_height()==height && display_stride()==stride,"Metadata mismatch");
 require(reads==4,"Geometry must be read and cached once");
 for(unsigned y=0;y<height;++y)for(unsigned x=0;x<stride;++x)
  require(pixels[y*stride+x]==(x<width?0x1c:0xa5),"Clear/row padding mismatch");
 rect(-2,-2,4,4,0xe0);pixel(width-1,height-1,3);pixel(-1,0,0);pixel(width,0,0);
 require(pixels[0]==0xe0 && pixels[stride+1]==0xe0 && pixels[2]==0x1c,"Clipping mismatch");
 require(pixels[(height-1)*stride+width-1]==3,"Last pixel mismatch");
 require(pixels[height*stride]==0xa5,"Write past display");
 unsigned before=writes;rect(0,0,-1,8,0);rect(0,0,8,-1,0);rect(-2147483647,0,1,1,0);
 require(before==writes,"Empty/off-screen rectangle wrote pixels");
 puts("PASS metadata, odd stride, aligned stores, clipping and full larger surface");return 0;
}
