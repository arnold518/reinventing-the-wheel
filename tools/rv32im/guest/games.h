#ifndef CIRCUITSIM_GAMES_H
#define CIRCUITSIM_GAMES_H
#include <stdint.h>
enum { GAME_2048=1, GAME_SOKOBAN, GAME_SNAKE, GAME_PONG, GAME_TETRIS };
enum { GAME_PLAYING, GAME_WON, GAME_LOST };
// Fixed-width state is part of the guest report ABI, independent of host ABI.
typedef struct {
    uint32_t kind, score, status, seed, paused, steps;
    int32_t x,y,dx,dy, other, ball_x,ball_y,ball_dx,ball_dy, piece,rotation;
    uint8_t cells[128], body_x[64], body_y[64];
    uint32_t length, food_x, food_y;
} Game;
void game_init(Game *g,uint32_t kind);
void game_action(Game *g,unsigned button,uint32_t levels);
uint8_t game_cell(const Game *g,unsigned x,unsigned y);
// Every cell of the drawn board (2048 4x4, Sokoban/Snake 8x8, Tetris 8x16).
void game_board(const Game *g,uint8_t *out);
uint32_t game_fingerprint(const Game *g);
#endif
