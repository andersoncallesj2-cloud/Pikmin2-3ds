#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "clip_shbin.h"
#include "vshader_shbin.h"

/* Independent control, not the game renderer. VBOs are immutable after init.
 * Projection path follows devkitPro simple_tri; clip tests bypass it entirely. */
#define TRANSFER (GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
static const float clip_pos[3][4]={{-.65f,-.55f,-.5f,1},{.65f,-.55f,-.5f,1},{0,.65f,-.5f,1}};
static const float clip_color[3][8]={
    {-.65f,-.55f,-.5f,1,1,0,0,1},{.65f,-.55f,-.5f,1,0,1,0,1},{0,.65f,-.5f,1,0,0,1,1}};
static const float ortho_pos[3][3]={{200,200,.5f},{100,40,.5f},{300,40,.5f}};
static const float persp_pos[3][3]={{-.7f,-.5f,-2},{.7f,-.5f,-2},{0,.7f,-2}};
static const char *names[]={"","CLEAR ONLY","CLIP / FIXED COLOR","CLIP / RGB COLOR","ORTHO / FIXED COLOR","PERSP / FIXED COLOR"};
static DVLB_s *binary[2];
static shaderProgram_s program[2];
static bool program_ok[2],c3d_ok,stalled,exit_requested;
static C3D_RenderTarget *target;
static unsigned char *vbo;
static C3D_Mtx ortho,persp;
static int uniform=-1;
#ifndef HW03_START_TEST
#define HW03_START_TEST 1
#endif
_Static_assert(HW03_START_TEST>=1 && HW03_START_TEST<=5,"Invalid HW03 startup test");
static unsigned test=HW03_START_TEST,last_test,submitted,completed,heartbeat,busy_ms;
static bool pending;
static u64 busy_start;
static FILE *log_file;
static const char *error="none";
static Result result;

static void record(const char *event)
{
    if(!log_file)return;
    fprintf(log_file,"[HW03] %s time=%llu selected=%u last_submit=%u submitted=%u completed=%u busy_ms=%u stalled=%u exit=%u phys=%08lx result=%08lx error=%s\n",
        event,(unsigned long long)osGetTime(),test,last_test,submitted,completed,busy_ms,stalled,exit_requested,
        (unsigned long)(vbo?osConvertVirtToPhys(vbo):0),(unsigned long)result,error);
    fflush(log_file);
}
static bool init_program(unsigned i,const void *data,unsigned size)
{
    binary[i]=DVLB_ParseFile((u32*)data,size);
    if(!binary[i] || binary[i]->numDVLE!=1)return false;
    shaderProgramInit(&program[i]);
    result=shaderProgramSetVsh(&program[i],&binary[i]->DVLE[0]);
    program_ok[i]=R_SUCCEEDED(result); return program_ok[i];
}
static bool initialize(void)
{
    c3d_ok=C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    if(!c3d_ok) { error="C3D_Init failed"; return false; }
    target=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
    if(!target) { error="target allocation failed"; return false; }
    C3D_RenderTargetSetOutput(target,GFX_TOP,GFX_LEFT,TRANSFER);
    if(!init_program(0,clip_shbin,clip_shbin_size) || !init_program(1,vshader_shbin,vshader_shbin_size)) {
        error="shader setup failed"; return false;
    }
    uniform=shaderInstanceGetUniformLocation(program[1].vertexShader,"projection");
    if(uniform<0) { error="projection uniform missing"; return false; }
    vbo=linearMemAlign(512,128);
    if(!vbo) { error="VBO allocation failed"; return false; }
    memset(vbo,0,512);
    memcpy(vbo,clip_pos,sizeof(clip_pos));
    memcpy(vbo+128,clip_color,sizeof(clip_color));
    memcpy(vbo+256,ortho_pos,sizeof(ortho_pos));
    memcpy(vbo+384,persp_pos,sizeof(persp_pos));
    result=GSPGPU_FlushDataCache(vbo,512);
    if(R_FAILED(result)) { error="VBO flush failed"; return false; }
    Mtx_OrthoTilt(&ortho,0,400,0,240,0,1,true);
    Mtx_PerspTilt(&persp,55.f*(3.14159265f/180.f),400.f/240.f,.1f,100.f,false);
    if(log_file) {
        for(unsigned i=0;i<2;i++)fprintf(log_file,"shader %u code=%lu outputs=%lu\n",i,(unsigned long)binary[i]->DVLP.codeSize,(unsigned long)binary[i]->DVLE[0].outmapData[0]);
        for(unsigned i=0;i<16;i++)fprintf(log_file,"matrix[%u] ortho=%g persp=%g\n",i,((float*)&ortho)[i],((float*)&persp)[i]);
    }
    return true;
}
static bool configure(void)
{
    bool rgb=test==3;
    bool projected=test>=4;
    C3D_BindProgram(&program[projected?1:0]);
    C3D_AttrInfo *a=C3D_GetAttrInfo(); AttrInfo_Init(a);
    int p=AttrInfo_AddLoader(a,0,GPU_FLOAT,projected?3:4);
    int c=rgb?AttrInfo_AddLoader(a,1,GPU_FLOAT,4):AttrInfo_AddFixed(a,1);
    if(!rgb)C3D_FixedAttribSet(1,1,1,1,1);
    C3D_BufInfo *b=C3D_GetBufInfo(); BufInfo_Init(b);
    unsigned offset=test==2?0:test==3?128:test==4?256:384;
    int id=BufInfo_Add(b,vbo+offset,rgb?32:projected?12:16,rgb?2:1,rgb?0x10:0);
    if(p<0 || c<0 || id<0) { error="attribute/buffer setup failed"; return false; }
    C3D_TexEnv *env=C3D_GetTexEnv(0); C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0);
    C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
    C3D_CullFace(GPU_CULL_NONE);
    if(projected)C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,uniform,test==4?&ortho:&persp);
    return true;
}
static void poll_frame(bool ready)
{
    if(!ready || stalled)return;
    if(!C3D_FrameBegin(C3D_FRAME_NONBLOCK)) {
        if(!busy_start)busy_start=osGetTime();
        busy_ms=(unsigned)(osGetTime()-busy_start);
        if(busy_ms>=2000) { stalled=true; error="GPU queue has not completed"; record("STALL LATCHED"); }
        return;
    }
    if(pending) { ++completed; pending=false; }
    busy_start=0; busy_ms=0;
    C3D_RenderTargetClear(target,C3D_CLEAR_ALL,test==1?0xC02080FF:0x233C36FF,0);
    if(!C3D_FrameDrawOn(target))error="FrameDrawOn failed";
    else if(test!=1 && configure())C3D_DrawArrays(GPU_TRIANGLES,0,3);
    C3D_FrameEnd(0);
    last_test=test; ++submitted; pending=true;
}
static void close_graphics(void)
{
    if(!c3d_ok)return;
    /* Drain with a bounded poll BEFORE freeing resources. A stalled queue does
     * not take Citro3D's indefinite wait or raw svcExitProcess path. */
    u64 start=osGetTime();
    while(!stalled) {
        if(C3D_FrameBegin(C3D_FRAME_NONBLOCK)) { C3D_FrameEnd(0); break; }
        if(osGetTime()-start>=250) { stalled=true; error="exit drain timed out"; break; }
        svcSleepThread(1000000);
    }
    record("EXIT CLEANUP");
    if(stalled)return; // Leave in-flight allocations for process teardown.
    C3D_Fini(); // Owns target; wait first, then free shader/VBO storage.
    for(unsigned i=0;i<2;i++) { if(program_ok[i])shaderProgramFree(&program[i]); if(binary[i])DVLB_Free(binary[i]); }
    if(vbo)linearFree(vbo);
}
int main(void)
{
    gfxInitDefault(); gfxSet3D(false); gfxSetDoubleBuffering(GFX_BOTTOM,false); consoleInit(GFX_BOTTOM,NULL);
    mkdir("sdmc:/3ds",0777); mkdir("sdmc:/3ds/pikmin2-hw03",0777);
    log_file=fopen("sdmc:/3ds/pikmin2-hw03/hardware.log","a");
    bool ready=initialize(); record("BOOT");
    bool changed=true; u64 last_log=0;
    while(aptMainLoop()) {
        hidScanInput(); u32 down=hidKeysDown();
        if(down&KEY_START) { exit_requested=true; record("START"); break; }
        if(!stalled && (down&KEY_DRIGHT)) { test=test%5+1; changed=true; }
        if(!stalled && (down&KEY_DLEFT)) { test=test==1?5:test-1; changed=true; }
        poll_frame(ready); ++heartbeat;
        if(changed || osGetTime()-last_log>=5000) { record("TICK"); last_log=osGetTime(); }
        if(changed || heartbeat%15==0) {
            consoleClear();
            printf("PIKMIN 2 HW03 STATIC GPU CONTROL\nNo ISO or gameplay code in this test.\n");
            printf("Setup:%s Test:%u/5\n%s\n",ready?"OK":"FAILED",test,names[test]);
            printf("Last submitted test: %u\nSubmit:%u Complete polls:%u\n",last_test,submitted,completed);
            printf("GPU busy:%ums STALLED:%s\nHeartbeat:%u\n",busy_ms,stalled?"YES":"no",heartbeat);
            printf("VBO:512 bytes, immutable\nPhysical:%08lx Uniform:%d\n",(unsigned long)(vbo?osConvertVirtToPhys(vbo):0),uniform);
            printf("Error:%.39s\nResult:%08lx\n",error,(unsigned long)result);
            printf("SD log:%s\n",log_file?"OPEN":"unavailable");
            printf("Left/Right: select  START: exit\n");
            if(stalled)printf("Tests locked after timeout.\nExit recovery is NOT guaranteed.\n");
            else printf("Wait 5 seconds at each stage.\nReport first distorted/frozen test.\n");
            printf("Hardware correction NOT yet verified.\n");
            gfxFlushBuffers(); changed=false;
        }
        svcSleepThread(16000000);
    }
    printf("\nExit requested; draining GPU...\n"); gfxFlushBuffers();
    close_graphics();
    if(log_file) { record("RETURN VIA LIBCTRU APT"); fclose(log_file); }
    if(!stalled)gfxExit();
    /* Return normally so libctru executes its APT close-application sequence.
     * With a genuinely wedged GPU the OS can still fail to recover. */
    return 0;
}
