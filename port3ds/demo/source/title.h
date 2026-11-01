#ifndef DEMO_TITLE_H
#define DEMO_TITLE_H
#include "game.h"
#include <stdint.h>
typedef struct {V3 pos;float face;unsigned color;bool active;} TitleUnit;
extern TitleUnit title_units[500];
extern V3 title_camera;
extern float title_fov,title_scale[2],title_time;
extern unsigned title_logo;
bool title_init(void);
void title_step(float,uint32_t);
#endif
