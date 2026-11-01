#ifndef P2_DEMO_GAME_H
#define P2_DEMO_GAME_H
#include <stdbool.h>
#include "camera.h"
#define SQUAD 100
#define MAX_ITEMS 32
#define MAX_ENEMIES 5
#define MAX_POSIES 16
#define MAX_HAZARDS 7
#define MAX_OBSTACLES 6
typedef struct { float x,y,z; } V3;
typedef enum { FOLLOW, AIR, IDLE, CARRY, ATTACK, LOST, HELD, UNUSED, SPROUT, SEED_AIR, ENTER_ONION, BURNING, DROWNING, DYING, DISBANDING } PikiState;
typedef struct { V3 pos,vel,disband_target; float face,work_time; PikiState state; int item,target; float settle;unsigned kind,owner;bool rejoin,called; } Piki;
typedef struct {V3 pos,home;float face,health,max_health,cooldown;bool dead,delivered;unsigned state,kind,item;} Bulborb;
typedef enum { ITEM_TREASURE,ITEM_CARCASS,ITEM_PELLET } ItemType;
typedef struct {V3 pos;int weight,max_carriers;bool available,delivered;float lift,radius,height;ItemType type;unsigned model,color,destination,value,seeds,matching_seeds;} CarryItem;
typedef struct {V3 pos;unsigned item;float health,scale;bool alive;} PelletPosy;
typedef struct {V3 pos;float radius;unsigned kind;} Hazard;
typedef struct {V3 pos;float half_x,half_z,height;} Obstacle;
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
    bool cycle_pressed,menu_accept,menu_cancel;
    int menu_delta;
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
    CarryItem items[MAX_ITEMS];
    Bulborb enemies[MAX_ENEMIES];
    PelletPosy posies[MAX_POSIES];
    Hazard hazards[MAX_HAZARDS];
    Obstacle obstacles[MAX_OBSTACLES];
    unsigned item_count,enemy_count,posy_count;
    int held_piki;
    unsigned onion_stored[3],selected_kind;
    bool other_following,plucking;
    V3 other_velocity;
    int onion_menu,onion_adjust;
    float pluck_timer;
    unsigned captain_action;float action_timer;
} Game;
void game_init(Game*,Parameters);
void game_step(Game*,Input,float);
unsigned game_count(const Game*,PikiState);
Input game_tour(float time);
unsigned game_carriers(const Game*,int);
void play_camera_init(Game*);
void play_camera_step(Game*,Input,float);
unsigned game_field_count(const Game*);
unsigned game_total_count(const Game*);
unsigned game_returnable(const Game*,unsigned);
V3 game_onion_position(unsigned);
V3 game_ship_position(void);
bool game_in_water(V3);
float game_day_ratio(const Game*);
unsigned game_day_number(const Game*);
unsigned game_squad_count(const Game*,unsigned);
#endif
