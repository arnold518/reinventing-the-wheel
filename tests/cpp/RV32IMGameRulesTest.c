#include "games.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"line %d: %s\n",__LINE__,#c);exit(1); } } while(0)
static void reset_check(Game *g) {Game initial;game_init(&initial,g->kind);game_action(g,5,0);CHECK(game_fingerprint(g)==game_fingerprint(&initial));}
int main(void) {
 Game g;
 game_init(&g,GAME_2048);memset(g.cells,0,sizeof(g.cells));
 g.cells[0]=g.cells[1]=g.cells[2]=g.cells[3]=1;game_action(&g,3,0);
 CHECK(g.cells[0]==2&&g.cells[1]==2&&g.score==8); // one merge per original tile
 game_init(&g,GAME_2048);memset(g.cells,0,sizeof(g.cells));g.cells[0]=g.cells[1]=10;game_action(&g,3,0);CHECK(g.status==GAME_WON&&g.score==2048);
 game_init(&g,GAME_2048);for(int y=0;y<4;++y)for(int x=0;x<4;++x)g.cells[y*4+x]=1+((x+y)&1);game_action(&g,3,0);CHECK(g.status==GAME_LOST);reset_check(&g);
 game_init(&g,GAME_SOKOBAN);game_action(&g,1,0);CHECK(g.status==GAME_WON&&g.score==1&&g.cells[28]==4);reset_check(&g);
 g.x=1;g.y=1;game_action(&g,3,0);CHECK(g.x==1&&g.y==1);g.cells[10]=2;g.cells[11]=1;game_action(&g,1,0);CHECK(g.x==1&&g.cells[10]==2);
 game_init(&g,GAME_SNAKE);game_action(&g,3,0);CHECK(g.dx==1&&g.dy==0); // reverse rejected
 game_action(&g,0,0);CHECK(g.dx==0&&g.dy==-1);game_action(&g,6,0);CHECK(g.body_x[0]==3&&g.body_y[0]==2);
 g.food_x=3;g.food_y=1;game_action(&g,6,0);CHECK(g.length==4&&g.score==1);game_action(&g,6,0);game_action(&g,6,0);CHECK(g.status==GAME_LOST);reset_check(&g);
 g.length=4;g.body_x[0]=1;g.body_y[0]=1;g.body_x[1]=2;g.body_y[1]=1;g.body_x[2]=2;g.body_y[2]=2;g.body_x[3]=1;g.body_y[3]=2;g.dx=0;g.dy=1;g.food_x=7;g.food_y=7;
 game_action(&g,6,0);CHECK(g.status==GAME_PLAYING&&g.body_y[0]==2); // vacating tail
 g.length=5;g.body_x[0]=1;g.body_y[0]=1;g.body_x[1]=1;g.body_y[1]=2;
 g.body_x[2]=2;g.body_y[2]=2;g.body_x[3]=2;g.body_y[3]=1;g.body_x[4]=2;g.body_y[4]=0;
 g.dx=0;g.dy=-1;game_action(&g,1,0);game_action(&g,6,0);CHECK(g.status==GAME_LOST);reset_check(&g);
 game_init(&g,GAME_PONG);game_action(&g,6,(1u<<0)|(1u<<8));CHECK(g.y==4&&g.other==6);
 g.ball_y=0;g.ball_dy=-1;game_action(&g,6,0);CHECK(g.ball_dy==1&&g.ball_y==1);
 g.ball_x=29;g.ball_dx=1;g.ball_y=g.other;g.ball_dy=1;game_action(&g,6,0);CHECK(g.ball_dx==-1&&g.status==GAME_PLAYING);
 g.ball_x=31;g.ball_dx=1;game_action(&g,6,0);CHECK(g.score==1&&g.status==GAME_WON);reset_check(&g);
 g.ball_x=0;g.ball_dx=-1;g.ball_y=14;g.ball_dy=1;game_action(&g,6,0);CHECK(g.score==1&&g.status==GAME_LOST);reset_check(&g);
 game_init(&g,GAME_TETRIS);g.piece=0;g.rotation=0;g.x=0;g.y=0;game_action(&g,0,0);CHECK(g.rotation==1);
 game_action(&g,3,0);CHECK(g.x==0); // left wall rotation preserved
 memset(g.cells,0,sizeof(g.cells));for(int x=0;x<4;++x)g.cells[15*8+x]=1;
 g.piece=0;g.rotation=0;g.x=4;g.y=0;game_action(&g,4,0);CHECK(g.score==100);
 for(int i=0;i<128;++i)CHECK(!g.cells[i]);
 for(int x=2;x<6;++x)g.cells[x]=1;
 g.piece=0;g.rotation=0;g.x=0;g.y=14;game_action(&g,4,0);CHECK(g.status==GAME_LOST);reset_check(&g);
 game_action(&g,9,0);CHECK(g.paused==1);int y=g.y;game_action(&g,6,0);CHECK(g.y==y);game_action(&g,9,0);game_action(&g,6,0);CHECK(g.y==y+1);
 // game_board draws the same cells as game_cell, through a few hundred moves of each game.
 for(unsigned kind=GAME_2048;kind<=GAME_TETRIS;++kind){
  game_init(&g,kind);unsigned seed=kind*7919u;
  const unsigned cols=kind==GAME_2048?4:8,rows=kind==GAME_2048?4:kind==GAME_TETRIS?16:8;
  for(int step=0;step<400;++step){
   uint8_t board[128];game_board(&g,board);
   if(kind!=GAME_PONG)for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<cols;++x)CHECK(board[y*cols+x]==game_cell(&g,x,y));
   seed=seed*1103515245u+12345u;unsigned b=(seed>>16)%7;if(b==5)b=6;
   if(g.status)game_action(&g,5,0);else game_action(&g,b,0);
  }
 }
 puts("PASS: merge, win/loss, pushes/walls, snake taps/food/tail/self/wall, two-player Pong/bounce/miss, Tetris rotation/line-clear/top-out, pause/restart; one-pass board equals per-cell queries");
 return 0;
}
