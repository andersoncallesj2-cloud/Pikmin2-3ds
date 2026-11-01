#ifndef P2_DEMO_GAME_H
#define P2_DEMO_GAME_H
#include <stdbool.h>
#define SQUAD 5
typedef struct { float x,y,z; } V3;
typedef enum { FOLLOW, AIR, IDLE, CARRY } PikiState;
typedef struct { V3 pos,vel; float face; PikiState state; } Piki;
typedef struct {
    float move,run,walk,gravity,throw_height,landing_time,call_max,call_min,call_time;
} Parameters;
typedef struct {
    float x,z,camera,aim_turn,aim_range;
    bool throw_pressed,whistle,dismiss,reset;
} Input;
typedef struct {
    V3 captain,velocity,cursor,crate;
    Piki pikis[SQUAD];
    float face,yaw,aim_angle,aim_distance,whistle_time,whistle_radius,time;
    unsigned throws,recalls,landings,delivered,steps;
    bool calling;
    Parameters parms;
} Game;
void game_init(Game*,Parameters);
void game_step(Game*,Input,float);
unsigned game_count(const Game*,PikiState);
Input game_tour(float time);
#endif
