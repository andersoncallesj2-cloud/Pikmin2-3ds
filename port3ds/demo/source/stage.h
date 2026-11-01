#ifndef DEMO_STAGE_H
#define DEMO_STAGE_H
#include <stdbool.h>
bool stage_init(void);
void stage_select(unsigned);
float stage_height(float x,float z,float hint);
bool stage_water(float x,float y,float z);
void stage_exit(void);
bool stage_landmark(unsigned,float out[4]);
bool stage_storage_point(unsigned,float out[3]);
#endif
