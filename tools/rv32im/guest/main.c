#include "platform.h"
#include "games.h"
#ifndef GAME_KIND
#define GAME_KIND 1
#endif
#ifndef GAME_ACTIONS
#define GAME_ACTIONS 3
#endif
#ifndef GAME_PRESET
#define GAME_PRESET 0
#endif
#ifndef GAME_AUTOTICKS
#define GAME_AUTOTICKS 0
#endif
static Game game;
static uint8_t previous[512];
#ifdef CIRCUITSIM_GUEST_REFERENCE
const Game *guest_reference_game_state(void){return &game;}
#endif
static const uint8_t colors[12]={0,224,28,3,252,227,31,124,156,208,180,255};
// Report at a fixed reserved address: magic, ABI, game, actions, score,
// terminal state, rule-state fingerprint, timer sample, overflow status.

static const uint16_t digits[10]={0x7b6f,0x2492,0x73e7,0x73cf,0x5bc9,0x79cf,0x79ef,0x7249,0x7bef,0x7bcf};
static void number(unsigned value,int x,int y) {
    unsigned n=1,divisor=1;while(value/divisor>=10){divisor*=10;++n;}
    for(unsigned pos=0;pos<n;++pos){unsigned digit=value/divisor;value%=divisor;divisor/=10;
        uint16_t glyph=digits[digit];
        for(int yy=0;yy<5;++yy)for(int xx=0;xx<3;++xx)if(glyph&(1u<<(14-yy*3-xx)))pixel(x+(int)pos*3+xx,y+yy,255);
    }
}
static void render(void) {
    if(GAME_KIND==GAME_PONG){
        static int old_left,old_right,old_x,old_y;static unsigned drawn;
        if(drawn){
            rect(old_x*2,old_y*3,2,3,0);
            if(old_left!=game.y)rect(2,old_left*3,2,12,0);
            if(old_right!=game.other)rect(60,old_right*3,2,12,0);
        }
        // Restore a paddle if the old ball could have covered its column.
        if(!drawn||old_left!=game.y||old_x==1)rect(2,game.y*3,2,12,colors[1]);
        if(!drawn||old_right!=game.other||old_x==30)rect(60,game.other*3,2,12,colors[2]);
        rect(game.ball_x*2,game.ball_y*3,2,3,colors[3]);
        old_left=game.y;old_right=game.other;old_x=game.ball_x;old_y=game.ball_y;drawn=1;
        present();return;
    }
    const unsigned cols=GAME_KIND==GAME_2048?4:GAME_KIND==GAME_PONG?32:8;
    const unsigned rows=GAME_KIND==GAME_2048?4:GAME_KIND==GAME_PONG||GAME_KIND==GAME_TETRIS?16:8;
    const unsigned cw=GAME_KIND==GAME_2048?16:GAME_KIND==GAME_PONG?2:GAME_KIND==GAME_TETRIS?4:6;
    const unsigned ch=GAME_KIND==GAME_2048?12:GAME_KIND==GAME_PONG||GAME_KIND==GAME_TETRIS?3:6;
    for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<cols;++x){
        unsigned at=y*cols+x;uint8_t v=game_cell(&game,x,y);if(previous[at]==v)continue;previous[at]=v;
        rect(x*cw,y*ch,cw,ch,colors[v%12]);
        if(GAME_KIND==GAME_2048&&v)number(1u<<v,(int)(x*cw)+1,(int)(y*ch)+3);
    }
    present();
}
int main(void) {
    volatile uint32_t *report=platform_report();
    console_write("CircuitSim RV32IM game\n");
    game_init(&game,GAME_KIND);
    // Curated acceptance boards are compiled into separate fixtures. The
    // ordinary five games retain their normal starting state.
#if GAME_PRESET == 1
    for(unsigned i=0;i<128;++i)game.cells[i]=0;
    for(unsigned i=0;i<4;++i)game.cells[i]=1;
#elif GAME_PRESET == 2
    for(unsigned i=0;i<128;++i)game.cells[i]=0;
    for(unsigned i=0;i<4;++i)game.cells[120+i]=1;
    game.piece=0;game.rotation=0;game.x=4;game.y=0;
#elif GAME_PRESET == 3
    game.ball_x=29;game.ball_y=5;game.ball_dx=game.ball_dy=1;
#elif GAME_PRESET == 4
    game.body_x[0]=6;game.body_x[1]=5;game.body_x[2]=4;
    game.food_x=0;game.food_y=0;
#endif
    // RST retains video memory, so startup still clears the surface. BSS
    // initializes previous[] to black: skip repainting black starting cells.
    // Later moves/restarts repaint every cell whose value changes.
    clear_screen(0);render();
    unsigned actions=0;
    while(actions<GAME_ACTIONS){
        if(!event_count())continue;
        uint32_t event=event_peek();event_pop();
        if(!(event&0x100u))continue;
        game_action(&game,event&31u,button_levels());++actions;render();
    }
    // A finite guest-controlled rally measures repeated real rule/render
    // updates without waiting for further device events or wall-clock time.
#if GAME_AUTOTICKS > 0
    for(unsigned i=0;i<GAME_AUTOTICKS;++i){game_action(&game,6,button_levels());++actions;render();}
#endif
    report[0]=0x4353474du;report[1]=1;report[2]=GAME_KIND;report[3]=actions;
    report[4]=game.score;report[5]=game.status;report[6]=game_fingerprint(&game);
    report[7]=ticks();report[8]=io_read(0x1000200cu);
    io_order();console_write("done\n");return 0;
}
