#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "diagnostic.h"
#include "port_engine.h"
#include "port_gx.h"

/* Resource probe only: original Section lifecycle/math, native resource I/O
 * and checked PICA backend. Original game/title/gameplay sections are not linked. */
static uint8_t pixels[96*32*3];
static bool loaded,romfs_ready,show_asset,colors=true,depth=true;
static char asset_error[80]="not loaded",identity[4][80];
static unsigned frames;
static uint32_t expected_crc;
static float orbit;
static PortMtx34 view;

static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint32_t crc32_bytes(const uint8_t *p,size_t size)
{
    uint32_t c=~0u;
    for(size_t i=0;i<size;i++) { c^=p[i]; for(int j=0;j<8;j++)c=(c>>1)^(0xedb88320u&-(c&1)); }
    return ~c;
}
static void load_asset(void)
{
    Result r=romfsInit();
    if(R_FAILED(r)) { snprintf(asset_error,sizeof(asset_error),"RomFS init failed: %08lx",(unsigned long)r); return; }
    romfs_ready=true;
    FILE *f=fopen("romfs:/banner.p2b","rb");
    if(!f) { snprintf(asset_error,sizeof(asset_error),"RomFS banner.p2b missing"); return; }
    uint8_t header[24];
    bool valid=fread(header,1,sizeof(header),f)==sizeof(header);
    valid=valid && !memcmp(header,"P2BN",4) && le32(header+4)==1 && le32(header+8)==96 && le32(header+12)==32 && le32(header+16)==sizeof(pixels);
    if(valid) {
        expected_crc=le32(header+20);
        valid=fread(pixels,1,sizeof(pixels),f)==sizeof(pixels) && fgetc(f)==EOF;
        valid=valid && crc32_bytes(pixels,sizeof(pixels))==expected_crc;
    }
    fclose(f);
    if(!valid) { snprintf(asset_error,sizeof(asset_error),"Asset size/header/CRC mismatch"); return; }
    loaded=true; snprintf(asset_error,sizeof(asset_error),"loaded: CRC32 %08lx",(unsigned long)expected_crc);
    f=fopen("romfs:/source.txt","r");
    if(f) {
        for(int i=0;i<4;i++)if(fgets(identity[i],sizeof(identity[i]),f))identity[i][strcspn(identity[i],"\r\n")]=0;
        fclose(f);
    }
}
static bool begin_frame(void *unused)
{
    (void)unused;
    if(!aptMainLoop())return false;
    hidScanInput(); u32 down=hidKeysDown(),held=hidKeysHeld();
    if(down&KEY_START)return false;
    if(down&KEY_A)show_asset=!show_asset;
    if(down&KEY_X)colors=!colors;
    if(down&KEY_Y)depth=!depth;
    if(held&KEY_L)orbit-=.01f;
    if(held&KEY_R)orbit+=.01f;
    if(down&KEY_B)orbit=0;
    PortVec3 eye={sinf(orbit)*14,8,cosf(orbit)*14},at={0,1,0},up={0,1,0};
    return port_math_camera(view,&eye,&up,&at);
}
static void begin_render(void *unused)
{ (void)unused; diag_options(show_asset?7:1,colors,depth); port_gfx_begin(view); }
static void draw(void *unused,PortEnginePhase phase)
{
    (void)unused; (void)phase;
    if(!show_asset || !loaded || !diag_ready() || diag_status()->frame!=CHECK_PASS)return;
    for(int z=-6;z<6;z++)for(int x=-6;x<6;x++) {
        uint32_t c=((x+z)&1)?0x496C3AFF:0x527B42FF;
        port_gfx_triangle((PortVec3){x,0,z},(PortVec3){x+1,0,z},(PortVec3){x,0,z+1},c);
        port_gfx_triangle((PortVec3){x+1,0,z},(PortVec3){x+1,0,z+1},(PortVec3){x,0,z+1},c);
    }
    port_graphics_sphere(view,(PortVec3){-3,.8f,1},.8f,0xE65745FF);
    port_graphics_sphere(view,(PortVec3){0,1,1},1,0xE9E1B5FF);
    port_graphics_sphere(view,(PortVec3){3,.8f,1},.8f,0x547DCBFF);
    /* Lossless nearest-pixel card: 96x32 RGB samples, two triangles per pixel.
     * This uses GPU color interpolation on constant-color quads, not a fake
     * texture upload. No new texture/shader path is needed for this milestone. */
    for(int y=0;y<32;y++)for(int x=0;x<96;x++) {
        size_t i=(y*96+x)*3;
        uint32_t c=(uint32_t)pixels[i]<<24|(uint32_t)pixels[i+1]<<16|(uint32_t)pixels[i+2]<<8|255;
        float l=-3.84f+x*.08f,r=l+.08f,t=5.f-y*.08f,b=t-.08f;
        port_gfx_triangle((PortVec3){l,t,-2},(PortVec3){r,t,-2},(PortVec3){l,b,-2},c);
        port_gfx_triangle((PortVec3){r,t,-2},(PortVec3){r,b,-2},(PortVec3){l,b,-2},c);
    }
}
static bool update(void *unused) { (void)unused; return true; }
static void end_render(void *unused) { (void)unused; port_gfx_end(); }
static void end_frame(void *unused)
{
    (void)unused;
    svcSleepThread(16000000);
    if(frames++%15)return;
    const DiagnosticStatus *s=diag_status(); consoleClear();
    printf("PIKMIN 2 ISO RESOURCE PROTOTYPE\n");
    printf("%-39.39s\n%-39.39s\n",identity[0],identity[1]);
    printf("Asset: opening.bnr (96 x 32)\n");
    printf("%-39.39s\n",asset_error);
    printf("Raw asset SHA256 (first 32):\n%.32s\n",identity[2]);
    printf("GPU:%s target:%s shader:%s\n",s->gpu==CHECK_PASS?"OK":"FAIL",s->target==CHECK_PASS?"OK":"FAIL",s->program==CHECK_PASS?"OK":"FAIL");
    printf("VBO:%s queue busy:%ums verts:%u\n",s->vbo==CHECK_PASS?"OK":"FAIL",s->busy_ms,s->vertices);
    printf("Frame %u GX errors:%u\n",frames,port_gfx_errors());
    printf("Mode: %s\n",show_asset?"ISO ASSET + SPHERES":"SAFE CLEAR COLOUR");
    printf("A: show/hide ISO-derived image\nL/R: orbit camera  B: reset\nX: color path  Y: depth\nSTART: exit\n");
    printf("Color:%s depth:%s\n",colors?"interleaved":"fixed",depth?"on":"off");
    printf("Linear free:%luK VRAM free:%luK\n",(unsigned long)linearSpaceFree()/1024,(unsigned long)vramSpaceFree()/1024);
    printf("Last: %.38s\n",s->last_error);
    printf("GPU image required; clear is not proof.\n");
    printf("Original Section + camera/math only.\nNo levels, characters or gameplay yet.\n");
    printf("Real New 3DS test still required.\n");
    gfxFlushBuffers();
}
int main(void)
{
    gfxInitDefault(); gfxSet3D(false); gfxSetDoubleBuffering(GFX_BOTTOM,false); consoleInit(GFX_BOTTOM,NULL);
    load_asset();
    bool gpu=port_gfx_init();
    if(!gpu) {
        printf("GPU initialization failed.\nAsset: %s\nSTART to exit.\n",asset_error); gfxFlushBuffers();
        while(aptMainLoop()) { hidScanInput(); if(hidKeysDown()&KEY_START)break; svcSleepThread(16000000); }
    } else {
        PortEngineCallbacks callbacks={0}; PortEngineStats stats={0};
        callbacks.begin_frame=begin_frame; callbacks.begin_render=begin_render; callbacks.draw=draw;
        callbacks.update=update; callbacks.end_render=end_render; callbacks.end_frame=end_frame;
        port_engine_run(&callbacks,0,0,&stats);
    }
    port_gfx_shutdown(); if(romfs_ready)romfsExit(); gfxExit(); return gpu && loaded?0:1;
}
