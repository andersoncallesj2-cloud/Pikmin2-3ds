#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>
#include "diagnostic.h"
#include "port_engine.h"
#include "port_gx.h"

/* Resource probe only: original Section lifecycle/math, native resource I/O
 * and checked PICA backend. Original game/title/gameplay sections are not linked. */
static uint8_t pixels[96*32*3];
static bool loaded,romfs_ready,colors,depth,changed=true;
static char asset_error[80]="not loaded",identity[4][80];
static unsigned frames;
static uint32_t expected_crc;
static float orbit;
static PortMtx34 view;
static unsigned test=1;
static FILE *log_file;
static u64 last_log,fps_start;
static unsigned fps_frames;
static float fps;
static const char *names[]={"","CLEAR COLOUR","TRIANGLE","CUBE","CHECKERBOARD","THREE SPHERES","FULL DEBUG SCENE","ISO BANNER"};
static const char *check(Check c) { return c==CHECK_PASS?"PASS":c==CHECK_FAIL?"FAIL":"not run"; }
static void select_test(unsigned next)
{ if(diag_status()->stalled)return; test=next; colors=test>=4; depth=test>=3; changed=true; }
static void record(void)
{
    if(!log_file)return;
    const DiagnosticStatus *s=diag_status();
    fprintf(log_file,"[ISO-PAL02] time=%llu test=%u %s asset=%s crc=%08lx GPU=%s RT=%s shader=%s program=%s uniform=%s VBO=%s attr=%s buffer=%s flush=%s frame=%s draw=%s verts=%u submitted=%u busy_ms=%u errors=%u last=%s\n",
        (unsigned long long)osGetTime(),test,names[test],loaded?"loaded":"FAILED",(unsigned long)expected_crc,
        check(s->gpu),check(s->target),check(s->shader),check(s->program),check(s->uniform),check(s->vbo),check(s->attributes),
        check(s->buffer),check(s->flush),check(s->frame),check(s->draw_on),s->vertices,s->submitted,s->busy_ms,s->failures,s->last_error);
    fflush(log_file);
    fprintf(log_file,"[ISO-PAL02] last_submitted_test=%u stalled=%u physical=%08lx\n",s->last_test,s->stalled,(unsigned long)s->physical);
    fflush(log_file);
}

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
    f=fopen("romfs:/start-test.txt","r");
    if(f) {
        int first=fgetc(f),newline=fgetc(f),end=fgetc(f);
        if(first>='1' && first<='7' && newline=='\n' && end==EOF)select_test((unsigned)(first-'0'));
        fclose(f);
    }
}
static bool begin_frame(void *unused)
{
    (void)unused;
    if(!aptMainLoop())return false;
    hidScanInput(); u32 down=hidKeysDown(),held=hidKeysHeld();
    if(down&KEY_START)return false;
    if(down&KEY_A)select_test(test==7?1:7);
    if(down&KEY_DRIGHT)select_test(test%7+1);
    if(down&KEY_DLEFT)select_test(test==1?7:test-1);
    if(down&KEY_X) { colors=!colors; changed=true; }
    if(down&KEY_Y) { depth=!depth; changed=true; }
    if(held&KEY_L)orbit-=.01f;
    if(held&KEY_R)orbit+=.01f;
    if(down&KEY_B)orbit=0;
    if(test==2) {
        const PortMtx34 identity_view={{1,0,0,0},{0,1,0,0},{0,0,1,0}};
        memcpy(view,identity_view,sizeof(view)); return true;
    }
    PortVec3 eye={sinf(orbit)*14,8,cosf(orbit)*14},at={0,1,0},up={0,1,0};
    return port_math_camera(view,&eye,&up,&at);
}
static void begin_render(void *unused)
{ (void)unused; diag_options(test,colors,depth); port_gfx_begin(view); }
static void draw(void *unused,PortEnginePhase phase)
{
    (void)unused; (void)phase;
    if(test==1 || !diag_ready() || diag_status()->frame!=CHECK_PASS)return;
    if(test==2)port_gfx_triangle((PortVec3){-.7f,-.5f,-2},(PortVec3){.7f,-.5f,-2},(PortVec3){0,.7f,-2},0xFFFFFFFF);
    if(test==3)port_gfx_box((PortVec3){0,1,0},1,1,1,0xFFE080FF);
    if(test==4 || test==6)for(int z=-6;z<6;z++)for(int x=-6;x<6;x++) {
        uint32_t c=((x+z)&1)?0x496C3AFF:0x527B42FF;
        port_gfx_triangle((PortVec3){x,0,z},(PortVec3){x+1,0,z},(PortVec3){x,0,z+1},c);
        port_gfx_triangle((PortVec3){x+1,0,z},(PortVec3){x+1,0,z+1},(PortVec3){x,0,z+1},c);
    }
    if(test==5 || test==6) {
        port_graphics_sphere(view,(PortVec3){-3,.8f,1},.8f,0xE65745FF);
        port_graphics_sphere(view,(PortVec3){0,1,1},1,0xE9E1B5FF);
        port_graphics_sphere(view,(PortVec3){3,.8f,1},.8f,0x547DCBFF);
    }
    if(test!=7 || !loaded)return;
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
    ++frames; ++fps_frames;
    u64 now=osGetTime();
    if(now-fps_start>=1000) { fps=fps_frames*1000.f/(now-fps_start); fps_frames=0; fps_start=now; }
    if(changed || now-last_log>=10000) { record(); last_log=now; }
    if(!changed && frames%15)return;
    changed=false;
    const DiagnosticStatus *s=diag_status(); consoleClear();
    printf("PIKMIN 2 ISO-PAL02 DIAGNOSTICS\n");
    printf("%-39.39s\n%-39.39s\n",identity[0],identity[1]);
    printf("Asset: opening.bnr (96 x 32)\n");
    printf("%-39.39s\n",asset_error);
    printf("Raw asset SHA256 (first 32):\n%.32s\n",identity[2]);
    printf("GPU:%s Target:%s\n",check(s->gpu),check(s->target));
    printf("Shader:%s Program:%s\n",check(s->shader),check(s->program));
    printf("Uniform:%s VBO:%s\n",check(s->uniform),check(s->vbo));
    printf("Attrib:%s Buffer:%s Flush:%s\n",check(s->attributes),check(s->buffer),check(s->flush));
    printf("Frame:%s DrawOn:%s busy:%ums\n",check(s->frame),check(s->draw_on),s->busy_ms);
    printf("Test %u/7: %s (%u verts)\n",test,names[test],s->vertices);
    printf("Last submit:%u Stalled:%s\n",s->last_test,s->stalled?"YES - tests locked":"no");
    printf("Heartbeat %u GX errors:%u\n",frames,port_gfx_errors());
    printf("Loop FPS:%.1f CPU:%.1f GPU:%.1fms\n",fps,s->cpu_ms,s->gpu_ms);
    printf("D-pad Left/Right: previous/next test\nA: banner/clear L/R: orbit B: reset\nX: color Y: depth START: exit\n");
    printf("Color:%s depth:%s\n",colors?"interleaved":"fixed",depth?"on":"off");
    printf("Linear free:%luK VRAM free:%luK\n",(unsigned long)linearSpaceFree()/1024,(unsigned long)vramSpaceFree()/1024);
    printf("Last: %.38s\n",s->last_error);
    printf("SD hardware.log: %s\n",log_file?"OPEN":"unavailable");
    printf("PASS = API check, not visible proof.\n");
    printf("PAL01 failed on hardware; PAL02 untested.\n");
    gfxFlushBuffers();
}
int main(void)
{
    gfxInitDefault(); gfxSet3D(false); gfxSetDoubleBuffering(GFX_BOTTOM,false); consoleInit(GFX_BOTTOM,NULL);
    mkdir("sdmc:/3ds",0777); mkdir("sdmc:/3ds/pikmin2-iso-prototype",0777);
    log_file=fopen("sdmc:/3ds/pikmin2-iso-prototype/hardware.log","a");
    fps_start=osGetTime();
    load_asset();
    if(log_file) { fprintf(log_file,"[ISO-PAL02] source=%s %s raw_sha256=%s asset=%s\n",identity[0],identity[1],identity[2],asset_error); fflush(log_file); }
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
    printf("\nExit requested; draining GPU...\n"); gfxFlushBuffers();
    record();
    port_gfx_shutdown();
    record(); if(log_file) { fprintf(log_file,"[ISO-PAL02] returning via normal libctru/APT exit\n"); fclose(log_file); }
    if(romfs_ready)romfsExit();
    if(!diag_status()->stalled)gfxExit();
    return gpu && loaded?0:1;
}
