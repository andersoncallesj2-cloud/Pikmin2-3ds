#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "port_engine.h"
#include "port_gx.h"
#include "port_math.h"
#include "port_audio.h"

/* Original Section lifecycle with a diagnostic payload, not replacement AI. */
typedef struct {
    PortMtx34 view;
    float focus_x,focus_z,orbit;
    bool bounds;
    unsigned frames;
    u64 previous;
    PortEnginePhase phase;
} EngineContext;

static bool begin_frame(void *opaque)
{
    EngineContext *c=opaque;
    if(!aptMainLoop()) return false;
    hidScanInput();
    u32 down=hidKeysDown(),held=hidKeysHeld();
    if(down&KEY_START) return false;
    if(down&KEY_B)c->bounds=!c->bounds;
    if(down&KEY_A)port_audio_play_whistle();
    if(down&KEY_Y) { c->focus_x=0; c->focus_z=0; c->orbit=0; }
    circlePosition stick; hidCircleRead(&stick);
    float x=stick.dx/156.f,z=-stick.dy/156.f;
    if(fabsf(x)<.12f)x=0;
    if(fabsf(z)<.12f)z=0;
    if(held&KEY_DLEFT)x=-1;
    if(held&KEY_DRIGHT)x=1;
    if(held&KEY_DUP)z=-1;
    if(held&KEY_DDOWN)z=1;
    u64 now=osGetTime();
    float dt=fminf(.05f,(now-c->previous)/1000.f); c->previous=now;
    c->focus_x=fmaxf(-5,fminf(5,c->focus_x+x*dt*3));
    c->focus_z=fmaxf(-5,fminf(5,c->focus_z+z*dt*3));
    if(held&KEY_L)c->orbit=port_math_turn_toward(c->orbit,c->orbit-.1f,dt);
    if(held&KEY_R)c->orbit=port_math_turn_toward(c->orbit,c->orbit+.1f,dt);
    PortVec3 eye={c->focus_x+sinf(c->orbit)*13,10,c->focus_z+cosf(c->orbit)*13};
    PortVec3 at={c->focus_x,0,c->focus_z},up={0,1,0};
    return port_math_camera(c->view,&eye,&up,&at);
}
static void begin_render(void *opaque) { port_gfx_begin(((EngineContext*)opaque)->view); }
static void draw(void *opaque,PortEnginePhase phase)
{
    EngineContext *c=opaque; c->phase=phase;
    for(int z=-6;z<6;z++) for(int x=-6;x<6;x++) {
        uint32_t color=((x+z)&1)?0x496C3AFF:0x527B42FF;
        port_gfx_triangle((PortVec3){x,0,z},(PortVec3){x+1,0,z},(PortVec3){x,0,z+1},color);
        port_gfx_triangle((PortVec3){x+1,0,z},(PortVec3){x+1,0,z+1},(PortVec3){x,0,z+1},color);
    }
    port_graphics_sphere(c->view,(PortVec3){0,1,0},1.f,0xE9E1B5FF);
    if(c->bounds) {
        port_graphics_sphere(c->view,(PortVec3){-3,.8f,0},.8f,0xE65745FF);
        port_graphics_sphere(c->view,(PortVec3){3,.8f,0},.8f,0x547DCBFF);
    }
}
static bool update(void *opaque) { (void)opaque; port_audio_update(); return true; }
static void end_render(void *opaque) { (void)opaque; port_gfx_end(); }
static void end_frame(void *opaque)
{
    EngineContext *c=opaque;
    if((c->frames++%15)==0) {
        printf("\x1b[15;0HOriginal Section phase: %d   frame %lu   \n",c->phase,(unsigned long)c->frames);
        printf("Focus %5.1f %5.1f   GX errors: %u       \n",c->focus_x,c->focus_z,port_gfx_errors());
        const char *status=port_audio_status(); size_t length=strlen(status);
        printf("%-39.39s\n",status);
        printf("%-39.39s\n",status+(length>39?39:length));
    }
}
int main(void)
{
    gfxInitDefault(); consoleInit(GFX_BOTTOM,NULL);
    printf("Pikmin 2 - original engine bring-up\nInitializing graphics...\n");
    if(!port_gfx_init()) {
        printf("GPU initialization failed.\nPress START to exit.\n");
        while(aptMainLoop()) { hidScanInput(); if(hidKeysDown()&KEY_START)break; gspWaitForVBlank(); }
        gfxExit(); return 1;
    }
    port_audio_init();
    EngineContext c={0}; c.bounds=true; c.previous=osGetTime();
    PortVec3 eye={0,10,13},at={0,0,0},up={0,1,0};
    port_math_camera(c.view,&eye,&up,&at);
    printf("\x1b[2J\x1b[0;0HPikmin 2: ORIGINAL Section loop\n\n");
    printf("Circle Pad / D-pad: camera focus\nL / R: orbit camera  Y: reset\nA: THP decoder audio test\nB: toggle original debug spheres\nSTART: exit\n\n");
    printf("Original: Section run/update/draw,\nGraphics primitives, camera/math,\nTHP movie-audio decoder.\n\n");
    printf("Boot/SingleGameSection not linked.\nNo original levels or Pikmin AI yet.\n");
    PortEngineCallbacks callbacks={0};
    callbacks.context=&c; callbacks.begin_frame=begin_frame; callbacks.begin_render=begin_render;
    callbacks.draw=draw; callbacks.update=update; callbacks.end_render=end_render; callbacks.end_frame=end_frame;
    PortEngineStats stats={0};
    bool ran=port_engine_run(&callbacks,0,0,&stats);
    port_audio_shutdown(); port_gfx_shutdown(); gfxExit(); return ran?0:1;
}
