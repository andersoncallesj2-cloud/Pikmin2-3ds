#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "diagnostic.h"
#include "port_gx.h"

static const char *names[]={"","CLEAR COLOUR","BASIC TRIANGLE","BASIC CUBE","CHECKERBOARD","THREE SPHERES","FULL DEBUG SCENE"};
static const char *check(Check c) { return c==CHECK_PASS?"PASS":c==CHECK_FAIL?"FAIL":"not run"; }
static FILE *log_file;
static void record(unsigned test,bool colors,bool depth)
{
    if(!log_file)return;
    const DiagnosticStatus *s=diag_status();
    fprintf(log_file,"[HW01] time=%llu test=%u %s colors=%s depth=%s GPU=%s RT=%s shader=%s program=%s uniform=%s VBO=%s attr=%s buffer=%s(id=%d) flush=%s frame=%s target=%s vertices=%u submitted=%u busy_ms=%u phys=%08lx linear_free=%lu vram_free=%lu api_result=%08lx last=%s\n",
        (unsigned long long)osGetTime(),test,names[test],colors?"interleaved":"fixed",depth?"on":"off",
        check(s->gpu),check(s->target),check(s->shader),check(s->program),check(s->uniform),check(s->vbo),check(s->attributes),check(s->buffer),s->buffer_id,
        check(s->flush),check(s->frame),check(s->draw_on),s->vertices,s->submitted,s->busy_ms,(unsigned long)s->physical,
        (unsigned long)linearSpaceFree(),(unsigned long)vramSpaceFree(),(unsigned long)s->last_result,s->last_error);
    fflush(log_file);
}
static void geometry(unsigned test,const PortMtx34 view)
{
    if(test==2) {
        /* Eye-space triangle; no original camera or resource dependencies. */
        port_gfx_triangle((PortVec3){-.7f,-.5f,-2},(PortVec3){.7f,-.5f,-2},(PortVec3){0,.7f,-2},0xFFFFFFFF);
    }
    if(test==3)port_gfx_box((PortVec3){0,1,0},1,1,1,0xFFE080FF);
    if(test==4 || test==6)for(int z=-6;z<6;z++)for(int x=-6;x<6;x++) {
        uint32_t color=((x+z)&1)?0x496C3AFF:0x527B42FF;
        port_gfx_triangle((PortVec3){x,0,z},(PortVec3){x+1,0,z},(PortVec3){x,0,z+1},color);
        port_gfx_triangle((PortVec3){x+1,0,z},(PortVec3){x+1,0,z+1},(PortVec3){x,0,z+1},color);
    }
    if(test==5 || test==6) {
        port_graphics_sphere(view,(PortVec3){0,1,0},1,0xE9E1B5FF);
        port_graphics_sphere(view,(PortVec3){-3,.8f,0},.8f,0xE65745FF);
        port_graphics_sphere(view,(PortVec3){3,.8f,0},.8f,0x547DCBFF);
    }
}
int main(void)
{
    gfxInitDefault(); gfxSet3D(false); gfxSetDoubleBuffering(GFX_BOTTOM,false); consoleInit(GFX_BOTTOM,NULL);
    printf("PIKMIN 2 HW01 - starting GPU checks\n");
    bool is_new=false; Result platform_result=APT_CheckNew3DS(&is_new);
    mkdir("sdmc:/3ds",0777); mkdir("sdmc:/3ds/pikmin2-hwdiag",0777);
    log_file=fopen("sdmc:/3ds/pikmin2-hwdiag/hardware.log","a");
    bool clear_ready=port_gfx_init();
    unsigned test=1,frames=0,loops=0;
    bool interleaved=false,depth=false,changed=true;
    float fps=0;
    u64 start=osGetTime(),last_log=start;
    while(aptMainLoop()) {
        hidScanInput(); u32 pressed=hidKeysDown();
        if(pressed&KEY_START)break;
        if(pressed&(KEY_R|KEY_DRIGHT)) { test=test%6+1; interleaved=test>=4; depth=test>=3; changed=true; }
        if(pressed&(KEY_L|KEY_DLEFT)) { test=test==1?6:test-1; interleaved=test>=4; depth=test>=3; changed=true; }
        if(pressed&KEY_X) { interleaved=!interleaved; changed=true; }
        if(pressed&KEY_A) { depth=!depth; changed=true; }
        PortMtx34 view={{1,0,0,0},{0,1,0,0},{0,0,1,0}};
        if(test>=3) {
            PortVec3 eye={0,10,13},target={0,0,0},up={0,1,0};
            port_math_camera(view,&eye,&up,&target);
        }
        diag_options(test,interleaved,depth);
        if(clear_ready) { port_gfx_begin(view); if(diag_ready() && diag_status()->frame==CHECK_PASS)geometry(test,view); port_gfx_end(); }
        const DiagnosticStatus *s=diag_status();
        svcSleepThread(16000000); /* No indefinite GSP wait on the input/UI path. */
        ++frames; ++loops;
        u64 now=osGetTime();
        if(now-start>=1000) { fps=loops*1000.f/(now-start); loops=0; start=now; }
        if(changed || now-last_log>=10000) { record(test,interleaved,depth); last_log=now; }
        if(changed || frames%15==0) {
            consoleClear();
            printf("PIKMIN 2 3DS DEBUG HW01\n");
            printf("Platform: %-27s\n",R_FAILED(platform_result)?"query failed":is_new?"New Nintendo 3DS":"Old 3DS / reported old");
            printf("Graphics: Citro3D / PICA200           \n");
            printf("Test %u/6: %-27s\n",test,names[test]);
            printf("GPU init: %-7s  Target: %-7s\n",check(s->gpu),check(s->target));
            printf("Shader parse: %-7s Program: %-7s\n",check(s->shader),check(s->program));
            printf("Uniform: %-7s VBO: %-7s          \n",check(s->uniform),check(s->vbo));
            printf("Attribs: %-7s Buffer: %-7s        \n",check(s->attributes),check(s->buffer));
            printf("Flush: %-7s Frame: %-7s           \n",check(s->flush),check(s->frame));
            printf("DrawOn: %-7s vertices: %-6u       \n",check(s->draw_on),s->vertices);
            printf("X color: %-12s A depth: %-3s       \n",interleaved?"interleaved":"FIXED",depth?"ON":"OFF");
            printf("Loop fps:%5.1f CPU:%4.1f GPU:%4.1fms\n",fps,s->cpu_ms,s->gpu_ms);
            printf("Heartbeat loop: %-10u              \n",frames);
            printf("VBO %lu KiB stride %lu phys %08lx\n",(unsigned long)s->bytes/1024,(unsigned long)s->stride,(unsigned long)s->physical);
            printf("Free linear:%luK VRAM:%luK           \n",(unsigned long)linearSpaceFree()/1024,(unsigned long)vramSpaceFree()/1024);
            printf("Submit:%u busy:%ums API errs:%u      \n",s->submitted,s->busy_ms,s->failures);
            printf("Last API result: %08lx               \n",(unsigned long)s->last_result);
            printf("Last: %-31.31s\n",s->last_error);
            printf("%-37.37s\n",strlen(s->last_error)>31?s->last_error+31:"");
            printf("SD log: %-28s\n",log_file?"hardware.log OPEN":"unavailable");
            printf("R/Right next | L/Left previous       \nX color path | A depth | START exit  \n");
            printf("PASS = API check; NOT visible proof. \nPlease report first blank test.      \n");
            gfxFlushBuffers(); /* Bottom HUD must update even if the GPU queue is stuck. */
            changed=false;
        }
    }
    record(test,interleaved,depth);
    if(log_file)fclose(log_file);
    port_gfx_shutdown(); gfxExit(); return 0;
}
