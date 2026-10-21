/* Audit validation: exercise long runs under sanitizers and dump every pixel
 * so incremental and full rendering can be compared byte for byte. */
#define AUDIT_HOST 1
#define main workload_main
#include "game-kernels.c"
#undef main
int main(void) {
 for(int k=0;k<7;++k) for(int r=0;r<4;++r) {
  unsigned count=0;
  for(int b=0;b<16;++b) count+=(shapes[k][r]>>b)&1;
  if(count!=4) return 10;
 }
 if(selfcheck()) return 11;
 clear_pixels();
 for(unsigned n=0;n<=FRAMES;++n) {
  run_frame(n);
  uint32_t state[]={frames,points,locks,(uint32_t)bx,(uint32_t)by,(uint32_t)vx,(uint32_t)vy,
   (uint32_t)paddle,(uint32_t)opponent,(uint32_t)tx,(uint32_t)ty,(uint32_t)rotation,(uint32_t)kind};
  if(fwrite(pixels,1,sizeof(pixels),stdout)!=sizeof(pixels)) return 12;
  if(fwrite(board,1,sizeof(board),stdout)!=sizeof(board)) return 13;
  if(fwrite(state,1,sizeof(state),stdout)!=sizeof(state)) return 14;
 }
 return 0;
}
