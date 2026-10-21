/* Original, deterministic headless workloads for the playability audit.
 * These are small game prototypes, not upstream game ports or device drivers.
 * GAME: 0=Pong, 1=Tetris movement, 2=Tetris line-clear stress.
 * RENDER: 0=logic only, 1=incremental pixels, 2=full framebuffer redraw.
 */
#include <stdint.h>
#ifdef AUDIT_HOST
#include <stdio.h>
#endif
#ifndef GAME
#define GAME 0
#endif
#ifndef RENDER
#define RENDER 1
#endif
#ifndef WIDTH
#define WIDTH 64
#endif
#ifndef HEIGHT
#define HEIGHT 48
#endif
#ifndef FRAMES
#define FRAMES 8
#endif
#ifndef EXPECTED
#define EXPECTED 0
#endif
#define NOINLINE __attribute__((noinline))
static uint8_t pixels[WIDTH * HEIGHT] __attribute__((aligned(4)));
static uint8_t board[20][10];
static uint32_t frames, points, locks;
static int bx=WIDTH/2, by=HEIGHT/2, vx=1, vy=1, paddle=HEIGHT/2, opponent=HEIGHT/2;
static int tx=3, ty=0, rotation=0, kind=0;
static const uint16_t shapes[7][4] = {
 {0x00f0,0x4444,0x0f00,0x2222}, {0x0066,0x0066,0x0066,0x0066},
 {0x0072,0x0262,0x0270,0x0232}, {0x0036,0x0462,0x0360,0x0231},
 {0x0063,0x0264,0x0630,0x0132}, {0x0071,0x0226,0x0470,0x0322},
 {0x0074,0x0622,0x0170,0x0223}
};
static int clamp(int n,int lo,int hi) {return n<lo?lo:n>hi?hi:n;}
static void rect(int x,int y,int w,int h,uint8_t color) {
 for(int j=0;j<h;++j) for(int i=0;i<w;++i)
  if(x+i>=0&&x+i<WIDTH&&y+j>=0&&y+j<HEIGHT) pixels[(y+j)*WIDTH+x+i]=color;
}
static void clear_pixels(void) {
 uint32_t *p=(uint32_t *)(void *)pixels;
 for(unsigned i=0;i<sizeof(pixels)/4;++i) p[i]=0;
}
static int fits(int x,int y,int r,int k) {
 uint16_t shape=shapes[k][r&3];
 for(int row=0;row<4;++row) for(int col=0;col<4;++col)
  if(shape&(1u<<(row*4+col))) {
   int xx=x+col, yy=y+row;
   if(xx<0||xx>=10||yy<0||yy>=20||board[yy][xx]) return 0;
  }
 return 1;
}
static unsigned clear_rows(void) {
 unsigned cleared=0;
 for(int row=19;row>=0;--row) {
  int full=1;
  for(int col=0;col<10;++col) if(!board[row][col]) full=0;
  if(full) {
   ++cleared;
   for(int y=row;y>0;--y) for(int x=0;x<10;++x) board[y][x]=board[y-1][x];
   for(int x=0;x<10;++x) board[0][x]=0;
   ++row;
  }
 }
 return cleared;
}
static void cell(int x,int y,uint8_t color) {rect(2+x*2,2+y*2,2,2,color);}
static void draw_piece(int x,int y,int r,int k,int erase) {
 uint16_t shape=shapes[k][r&3];
 for(int row=0;row<4;++row) for(int col=0;col<4;++col)
  if(shape&(1u<<(row*4+col))) {
   int xx=x+col,yy=y+row;
   if(xx>=0&&xx<10&&yy>=0&&yy<20) cell(xx,yy,erase?board[yy][xx]:(uint8_t)(k+1));
  }
}
static void draw_board(void) {
 for(int y=0;y<20;++y) for(int x=0;x<10;++x) cell(x,y,board[y][x]);
}
static NOINLINE void pong_frame(unsigned input) {
 if(RENDER==1) {
  rect(bx,by,2,2,0); rect(2,paddle-4,2,8,0); rect(WIDTH-4,opponent-4,2,8,0);
 }
 if(input&1) paddle-=2;
 if(input&2) paddle+=2;
 paddle=clamp(paddle,4,HEIGHT-4);
 opponent=clamp(opponent+(by>opponent?1:-1),4,HEIGHT-4);
 bx+=vx; by+=vy;
 if(by<=0||by>=HEIGHT-2) {vy=-vy;by=clamp(by,0,HEIGHT-2);}
 if(bx<=4&&vx<0&&by+2>=paddle-4&&by<=paddle+4) {vx=1;bx=4;}
 if(bx>=WIDTH-6&&vx>0&&by+2>=opponent-4&&by<=opponent+4) {vx=-1;bx=WIDTH-6;}
 if(bx<0||bx>=WIDTH-2) {++points;bx=WIDTH/2;by=HEIGHT/2;vx=-vx;}
 if(RENDER==2) clear_pixels();
 if(RENDER) {rect(bx,by,2,2,7);rect(2,paddle-4,2,8,2);rect(WIDTH-4,opponent-4,2,8,3);}
}
static NOINLINE void tetris_frame(unsigned input) {
 if(GAME==2) {
  /* Force a horizontal I piece to complete the bottom row every update. */
  for(int y=0;y<20;++y) for(int x=0;x<10;++x) board[y][x]=y==19&&(x<3||x>6);
  kind=0;rotation=0;tx=3;ty=18;
 }
 if(RENDER==1) draw_piece(tx,ty,rotation,kind,1);
 if((input&1)&&fits(tx-1,ty,rotation,kind)) --tx;
 if((input&2)&&fits(tx+1,ty,rotation,kind)) ++tx;
 if((input&4)&&fits(tx,ty,rotation+1,kind)) rotation=(rotation+1)&3;
 int locked=0;
 if(fits(tx,ty+1,rotation,kind)) ++ty;
 else {
  uint16_t shape=shapes[kind][rotation];
  for(int r=0;r<4;++r) for(int c=0;c<4;++c)
   if(shape&(1u<<(r*4+c))) board[ty+r][tx+c]=(uint8_t)(kind+1);
  points+=clear_rows();++locks;locked=1;
  kind=(kind+1)%7;rotation=0;tx=3;ty=0;
  if(!fits(tx,ty,rotation,kind)) {
   for(int y=0;y<20;++y) for(int x=0;x<10;++x) board[y][x]=0;
  }
 }
 if(RENDER==2) {clear_pixels();draw_board();}
 if(RENDER==1&&locked) draw_board();
 if(RENDER) draw_piece(tx,ty,rotation,kind,0);
}
static NOINLINE void run_frame(unsigned n) {
 unsigned input=GAME==2?0:((n&7)<3?1:(n&7)<6?2:4);
 if(GAME==0) pong_frame(input); else tetris_frame(input);
 ++frames;
}
static NOINLINE uint32_t checksum(void) {
 uint32_t h=frames^(points<<8)^(locks<<16);
 h^=(uint32_t)(bx+by*WIDTH+paddle*16+opponent*256);
 h^=(uint32_t)(tx+ty*16+rotation*512+kind*2048);
 /* Bounded sampled pixel check; instruction lockstep checks all memory. */
 for(unsigned i=0;i<64;++i) h=((h<<5)|(h>>27))^pixels[(i*(WIDTH*HEIGHT/64))%(WIDTH*HEIGHT)];
 if(GAME) for(unsigned y=0;y<20;++y) for(unsigned x=0;x<10;++x) h=((h<<5)|(h>>27))^board[y][x];
 return h;
}
static int selfcheck(void) {
 /* Check collision boundaries, rotation and a two-row collapse separately. */
 if(fits(-5,0,0,0)||fits(3,20,0,0)||!fits(3,0,1,0)) return 1;
 for(int y=18;y<20;++y) for(int x=0;x<10;++x) board[y][x]=1;
 if(clear_rows()!=2) return 2;
 for(int y=0;y<20;++y) for(int x=0;x<10;++x) if(board[y][x]) return 3;
 if(GAME==0) {
  bx=WIDTH/2;by=0;vy=-1;pong_frame(0);
  if(vy!=1||by!=0) return 4;
  bx=WIDTH/2;by=HEIGHT/2;vx=vy=1;paddle=opponent=HEIGHT/2;
 }
 return 0;
}
int main(void) {
#ifdef AUDIT_HOST
 if(selfcheck()) return 1;
 clear_pixels();
#endif
 run_frame(0); /* BSS is zero; one warm-up update outside the ROI. */
#ifndef AUDIT_HOST
 __asm__ volatile(".global audit_begin\naudit_begin:" ::: "memory");
#endif
 for(unsigned n=1;n<=FRAMES;++n) run_frame(n);
#ifndef AUDIT_HOST
 __asm__ volatile(".global audit_end\naudit_end:" ::: "memory");
#endif
 uint32_t h=checksum();
#ifdef AUDIT_HOST
 printf("%u %u %u %u\n",h,frames,points,locks);
 return 0;
#else
 return h!=(uint32_t)EXPECTED;
#endif
}
