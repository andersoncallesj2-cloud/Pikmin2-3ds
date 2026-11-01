#ifndef P2_DEMO_GAME_H
#define P2_DEMO_GAME_H
#include <stdbool.h>
#include "camera.h"
#define SQUAD 20
typedef struct { float x,y,z; } V3;
typedef enum { FOLLOW, AIR, IDLE, CARRY, ATTACK, LOST, HELD } PikiState;
typedef struct { V3 pos,vel; float face,work_time; PikiState state; int item; float settle; } Piki;
typedef struct {V3 pos;float face,health,cooldown;bool dead,delivered;unsigned state;} Bulborb;
typedef struct {V3 pos;int weight,max_carriers;bool delivered;float lift;} CarryItem;
typedef struct {
    float move,run,walk,gravity,throw_height,landing_time,call_max,call_min,call_time;
    float shake,neutral,cursor_threshold,stick_clamp,cursor_radius,cursor_speed,cursor_look_time;
    CameraParameters camera[2];
    float enemy_health,enemy_speed,enemy_wake,enemy_attack_radius,enemy_damage;
    float treasure_weight,treasure_max,carcass_weight,carcass_max,treasure_value;
    float carcass_yield,captain_max_health,punch_damage,punch_range;
} Parameters;
typedef struct {
    float x,z,camera,aim_turn,aim_range;
    bool throw_pressed,whistle,dismiss,reset;
    bool center_pressed,center_held,zoom_pressed,zoom_held,angle_pressed,switch_pressed;
    bool grab_pressed,grab_held;
    float swarm_x,swarm_z;
} Input;
typedef struct {
    V3 captain,velocity,cursor,crate;
    Piki pikis[SQUAD];
    float face,yaw,aim_angle,aim_distance,whistle_time,whistle_radius,time;
    unsigned throws,recalls,landings,delivered,steps;
    bool calling;
    Parameters parms;
    PlayCamera camera;
    V3 cursor_offset,other_captain,other_cursor;
    float other_face,still_time,swarm_angle,swarm_strength,captain_health[2];
    unsigned active_captain,deaths,population,pokos;
    CarryItem items[2];
    Bulborb enemy;
    int held_piki;
    unsigned onion_stored;
} Game;
void game_init(Game*,Parameters);
void game_step(Game*,Input,float);
unsigned game_count(const Game*,PikiState);
Input game_tour(float time);
unsigned game_carriers(const Game*,int);
void play_camera_init(Game*);
void play_camera_step(Game*,Input,float);
#endif
