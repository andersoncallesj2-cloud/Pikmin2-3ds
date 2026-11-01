#ifndef P2_ORIGINAL_UI_H
#define P2_ORIGINAL_UI_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*OriginalGlyph)(void*,float,float,float,float,int,unsigned char,unsigned char,unsigned char,unsigned char);
void original_ui_reset(void);
void original_health_tick(unsigned,float,float);
unsigned original_health_segments(unsigned);
void original_health_color(unsigned,unsigned char[4]);
unsigned original_health_geometry(float*);
void original_carry_tick(unsigned,float,bool,float);
void original_carry_pose(unsigned,float*,float*,float*);
void original_carry_draw(unsigned,unsigned,unsigned,unsigned,OriginalGlyph,void*);
#ifdef __cplusplus
}
#endif
#endif
