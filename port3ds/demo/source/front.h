#ifndef DEMO_FRONT_H
#define DEMO_FRONT_H
#include "game.h"
#include <stdint.h>
enum {VIEW_PLAY,VIEW_TITLE,VIEW_MAPS,VIEW_PAUSE,VIEW_SETTINGS,VIEW_3DS,VIEW_TREASURE,VIEW_GAME_OVER,VIEW_TITLE_MENU,VIEW_UNAVAILABLE};
typedef struct {uint32_t magic;bool debug,stereo,hitboxes,audio;} Settings;
extern Settings settings;
extern unsigned front_view,front_choice,debug_page,debug_entity;
extern bool front_quit;
extern float front_time;
void front_init(void);
bool front_update(Game*,uint32_t down,uint32_t held,float dt);
void front_save(void);
bool front_title_scene(void);
#endif
