#include "games.h"
// Original small game rules. No host libraries, floating point or wall clock.
enum { UP,RIGHT,DOWN,LEFT,ACTION,RESET,TICK,P2_UP,P2_DOWN,PAUSE };
static const int vx[4]={0,1,0,-1},vy[4]={-1,0,1,0};
static uint32_t rng(Game *g) { uint32_t x=g->seed; x^=x<<13;x^=x>>17;x^=x<<5;return g->seed=x; }
// Word stores: Game holds only fixed-width fields and is 336 bytes long.
static void zero(Game *g) { uint32_t *p=(uint32_t *)g;for(unsigned i=0;i<sizeof(*g)/4;++i)p[i]=0; }
static void spawn2048(Game *g) {
    unsigned free=0;for(unsigned i=0;i<16;++i)free+=g->cells[i]==0;
    if(!free)return;
    unsigned slot=rng(g)%free;
    for(unsigned i=0;i<16;++i)if(!g->cells[i] && slot--==0){g->cells[i]=1;break;}
}
static const uint8_t level[64]={
 1,1,1,1,1,1,1,1, 1,0,0,0,0,0,0,1, 1,0,0,0,0,0,0,1, 1,0,0,2,3,0,0,1,
 1,0,0,0,0,0,0,1, 1,0,0,0,0,0,0,1, 1,0,0,0,0,0,0,1, 1,1,1,1,1,1,1,1};
// Sokoban: 1 wall, 2 crate, 3 goal, 4 crate on goal.
static const uint16_t shapes[7][4]={
 {0x000f,0x1111,0x000f,0x1111}, {0x0033,0x0033,0x0033,0x0033},
 {0x0027,0x0131,0x0072,0x0232}, {0x0036,0x0231,0x0036,0x0231},
 {0x0063,0x0132,0x0063,0x0132}, {0x0047,0x0113,0x0071,0x0322},
 {0x0017,0x0311,0x0074,0x0223}};
static int fit(const Game *g,int x,int y,int rotation) {
 uint16_t shape=shapes[g->piece][rotation&3];
 for(int j=0;j<4;++j)for(int i=0;i<4;++i)if(shape&(1u<<(j*4+i)))
   if(x+i<0||x+i>=8||y+j<0||y+j>=16||g->cells[(y+j)*8+x+i])return 0;
 return 1;
}
static void new_piece(Game *g) {g->piece=rng(g)%7;g->rotation=0;g->x=2;g->y=0;if(!fit(g,g->x,g->y,0))g->status=GAME_LOST;}
static void lock_piece(Game *g) {
 uint16_t shape=shapes[g->piece][g->rotation&3];
 for(int j=0;j<4;++j)for(int i=0;i<4;++i)if(shape&(1u<<(j*4+i)))g->cells[(g->y+j)*8+g->x+i]=(uint8_t)(g->piece+1);
 for(int y=15;y>=0;--y){int full=1;for(int x=0;x<8;++x)full&=g->cells[y*8+x]!=0;
  if(full){for(int yy=y;yy>0;--yy)for(int x=0;x<8;++x)g->cells[yy*8+x]=g->cells[(yy-1)*8+x];
   for(int x=0;x<8;++x)g->cells[x]=0;
   g->score+=100;++y;}}
 new_piece(g);
}
static void snake_food(Game *g) {
 if(g->length==64){g->status=GAME_WON;return;}
 unsigned start=rng(g)&63;
 for(unsigned k=0;k<64;++k){unsigned q=(start+k)&63;int occupied=0;
  for(unsigned i=0;i<g->length;++i)occupied|=(g->body_x[i]==(q&7)&&g->body_y[i]==(q>>3));
  if(!occupied){g->food_x=q&7;g->food_y=q>>3;return;}}
}
void game_init(Game *g,uint32_t kind) {
 zero(g);g->kind=kind;g->seed=0x12345678u;
 switch(kind){
 case GAME_2048:spawn2048(g);spawn2048(g);break;
 case GAME_SOKOBAN:for(unsigned i=0;i<64;++i)g->cells[i]=level[i];g->x=2;g->y=3;break;
 case GAME_SNAKE:g->length=3;g->dx=1;g->body_x[0]=3;g->body_y[0]=3;
  g->body_x[1]=2;g->body_y[1]=3;g->body_x[2]=1;g->body_y[2]=3;snake_food(g);break;
 case GAME_PONG:g->y=g->other=5;g->ball_x=16;g->ball_y=8;g->ball_dx=1;g->ball_dy=1;break;
 case GAME_TETRIS:new_piece(g);break;
 default:g->status=GAME_LOST;
 }
}
static void move2048(Game *g,unsigned d) {
 uint8_t before[16];for(int i=0;i<16;++i)before[i]=g->cells[i];
 for(int row=0;row<4;++row){int positions[4],n=0;uint8_t values[4]={0};
  for(int i=0;i<4;++i){int x=d==LEFT?i:d==RIGHT?3-i:row;int y=d==UP?i:d==DOWN?3-i:row;
   positions[i]=y*4+x;if(g->cells[positions[i]])values[n++]=g->cells[positions[i]];}
  int out=0;for(int i=0;i<n;++i){uint8_t v=values[i];if(i+1<n&&values[i+1]==v){++v;++i;g->score+=1u<<v;}
   g->cells[positions[out++]]=v;if(v>=11)g->status=GAME_WON;}
  while(out<4)g->cells[positions[out++]]=0;
 }
 int changed=0;for(int i=0;i<16;++i)changed|=before[i]!=g->cells[i];if(changed)spawn2048(g);
 int possible=0;for(int y=0;y<4;++y)for(int x=0;x<4;++x){int i=y*4+x;
  possible|=!g->cells[i]||(x<3&&g->cells[i]==g->cells[i+1])||(y<3&&g->cells[i]==g->cells[i+4]);}
 if(!possible&&g->status!=GAME_WON)g->status=GAME_LOST;
}
static void sokoban(Game *g,unsigned d) {
 int x=g->x+vx[d],y=g->y+vy[d];if((unsigned)x>=8||(unsigned)y>=8)return;
 int at=y*8+x,v=g->cells[at];if(v==1)return;
 if(v==2||v==4){int nx=x+vx[d],ny=y+vy[d];if((unsigned)nx>=8||(unsigned)ny>=8)return;
  int next=ny*8+nx,n=g->cells[next];if(n!=0&&n!=3)return;
  g->cells[next]=n==3?4:2;g->cells[at]=v==4?3:0;++g->score;}
 g->x=x;g->y=y;int remaining=0;for(int i=0;i<64;++i)remaining|=g->cells[i]==2;
 if(!remaining)g->status=GAME_WON;
}
static void snake(Game *g,unsigned b) {
 if(b<4){if(vx[b]!=-g->dx||vy[b]!=-g->dy){g->dx=vx[b];g->dy=vy[b];}return;}
 if(b!=TICK)return;
 int x=g->body_x[0]+g->dx,y=g->body_y[0]+g->dy;
 if((unsigned)x>=8||(unsigned)y>=8){g->status=GAME_LOST;return;}
 int eat=(unsigned)x==g->food_x&&(unsigned)y==g->food_y;
 // Moving into the tail is legal when it leaves on this update.
 for(unsigned i=0;i<g->length-(eat?0:1);++i)if(g->body_x[i]==x&&g->body_y[i]==y){g->status=GAME_LOST;return;}
 if(eat){++g->length;++g->score;}
 for(unsigned i=g->length-1;i>0;--i){g->body_x[i]=g->body_x[i-1];g->body_y[i]=g->body_y[i-1];}
 g->body_x[0]=x;g->body_y[0]=y;if(eat)snake_food(g);
}
static void pong(Game *g,unsigned b,uint32_t levels) {
 if(b!=TICK)return;
 g->y+=(levels&(1u<<DOWN)?1:0)-(levels&(1u<<UP)?1:0);
 g->other+=(levels&(1u<<P2_DOWN)?1:0)-(levels&(1u<<P2_UP)?1:0);
 if(g->y<0)g->y=0;
 if(g->y>12)g->y=12;
 if(g->other<0)g->other=0;
 if(g->other>12)g->other=12;
 int x=g->ball_x+g->ball_dx,y=g->ball_y+g->ball_dy;
 if(y<0||y>=16){g->ball_dy=-g->ball_dy;y=g->ball_y+g->ball_dy;}
 if(x==1&&g->ball_dx<0&&y>=g->y&&y<g->y+4){g->ball_dx=1;x=2;}
 if(x==30&&g->ball_dx>0&&y>=g->other&&y<g->other+4){g->ball_dx=-1;x=29;}
 if(x<0||x>=32){++g->score;g->status=x>=32?GAME_WON:GAME_LOST;return;}
 g->ball_x=x;g->ball_y=y;
}
static void tetris(Game *g,unsigned b) {
 if(b==LEFT||b==RIGHT){int x=g->x+(b==LEFT?-1:1);if(fit(g,x,g->y,g->rotation))g->x=x;}
 if(b==UP&&fit(g,g->x,g->y,g->rotation+1))g->rotation=(g->rotation+1)&3;
 if(b==ACTION){while(fit(g,g->x,g->y+1,g->rotation))++g->y;lock_piece(g);}
 else if(b==DOWN||b==TICK){if(fit(g,g->x,g->y+1,g->rotation))++g->y;else lock_piece(g);}
}
void game_action(Game *g,unsigned b,uint32_t levels) {
 if(b==RESET){uint32_t kind=g->kind;game_init(g,kind);return;}
 if(b==PAUSE){g->paused^=1;return;}if(g->status||g->paused)return;++g->steps;
 switch(g->kind){case GAME_2048:if(b<4)move2048(g,b);break;case GAME_SOKOBAN:if(b<4)sokoban(g,b);break;
 case GAME_SNAKE:snake(g,b);break;case GAME_PONG:pong(g,b,levels);break;case GAME_TETRIS:tetris(g,b);break;}
}
uint32_t game_fingerprint(const Game *g) {
 // Field-wise hash excludes padding; byte order is specified little-endian.
 uint32_t h=2166136261u;
 const uint32_t scalar[]={g->kind,g->score,g->status,g->seed,g->paused,g->steps,
  (uint32_t)g->x,(uint32_t)g->y,(uint32_t)g->dx,(uint32_t)g->dy,(uint32_t)g->other,
  (uint32_t)g->ball_x,(uint32_t)g->ball_y,(uint32_t)g->ball_dx,(uint32_t)g->ball_dy,(uint32_t)g->piece,(uint32_t)g->rotation,
  g->length,g->food_x,g->food_y};
 for(unsigned i=0;i<sizeof(scalar)/sizeof(scalar[0]);++i)for(unsigned b=0;b<4;++b){h^=(scalar[i]>>(8*b))&255;h*=16777619u;}
 for(unsigned i=0;i<128;++i){h^=g->cells[i];h*=16777619u;}
 for(unsigned i=0;i<64;++i){h^=g->body_x[i];h*=16777619u;h^=g->body_y[i];h*=16777619u;}
 return h;
}

uint8_t game_cell(const Game *g,unsigned x,unsigned y) {
 switch(g->kind){
 case GAME_2048:return x<4&&y<4?g->cells[y*4+x]:0;
 case GAME_SOKOBAN:if((int)x==g->x&&(int)y==g->y)return 5;return x<8&&y<8?g->cells[y*8+x]:0;
 case GAME_SNAKE:
  if(x==g->food_x&&y==g->food_y)return 3;
  for(unsigned i=0;i<g->length;++i)if(x==g->body_x[i]&&y==g->body_y[i])return i?2:1;
  return 0;
 case GAME_PONG:
  if(x==(unsigned)g->ball_x&&y==(unsigned)g->ball_y)return 3;
  if(x==1&&y>=(unsigned)g->y&&y<(unsigned)g->y+4)return 1;
  if(x==30&&y>=(unsigned)g->other&&y<(unsigned)g->other+4)return 2;
  return 0;
 case GAME_TETRIS:{
  int px=(int)x-g->x,py=(int)y-g->y;
  if(px>=0&&px<4&&py>=0&&py<4&&(shapes[g->piece][g->rotation]&(1u<<(py*4+px))))return g->piece+1;
  return x<8&&y<16?g->cells[y*8+x]:0;
 }
 default:return 0;
 }
}

// The whole board at once, row by row (cols x rows as drawn): the same values
// as game_cell, in one pass instead of one rule query per cell.
void game_board(const Game *g,uint8_t *out) {
 switch(g->kind){
 case GAME_2048:for(unsigned i=0;i<16;++i)out[i]=g->cells[i];return;
 case GAME_SOKOBAN:for(unsigned i=0;i<64;++i)out[i]=g->cells[i];
  if((unsigned)g->x<8&&(unsigned)g->y<8)out[g->y*8+g->x]=5;
  return;
 case GAME_SNAKE:for(unsigned i=0;i<64;++i)out[i]=0;
  // Later segments first, so the head and then the food win a shared cell.
  for(unsigned i=g->length;i-->0;)out[g->body_y[i]*8+g->body_x[i]]=i?2:1;
  if(g->food_x<8&&g->food_y<8)out[g->food_y*8+g->food_x]=3;
  return;
 case GAME_TETRIS:{
  for(unsigned i=0;i<128;++i)out[i]=g->cells[i];
  uint16_t shape=shapes[g->piece][g->rotation&3];
  for(int j=0;j<4;++j)for(int i=0;i<4;++i)if(shape&(1u<<(j*4+i))){
   int x=g->x+i,y=g->y+j;if(x>=0&&x<8&&y>=0&&y<16)out[y*8+x]=(uint8_t)(g->piece+1);}
  return;}
 default:return;
 }
}
