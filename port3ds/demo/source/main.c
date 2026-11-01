#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include "game.h"
#include "port_math.h"
#include "vshader_shbin.h"
#define TRANSFER (GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
typedef struct {float p[4],c[4],uv[4];} Vertex;
typedef struct {Vertex *v;u32 count;int texture;} Batch;
typedef struct {C3D_Tex tex[16];unsigned nt,nb;Batch batch[16];} Model;
static Model olimar,pikmin,leaf,onion;
static C3D_Tex white,ground;
static Vertex *plane,*cube,*ring;
static C3D_RenderTarget *target;
static shaderProgram_s program;
static DVLB_s *shader;
static C3D_Mtx projection,view;
static int u_mvp,u_tint;
static bool c3d,program_ok,stalled;
static const char *error="none";
static unsigned frames,busy_ms;
static u64 busy_start;
static FILE *log_file;
#ifndef DEMO_TOUR
#define DEMO_TOUR 0
#endif
static u32 crc32(const unsigned char *p,size_t n){u32 c=~0u;for(size_t i=0;i<n;i++){c^=p[i];for(int b=0;b<8;b++)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
static bool take(FILE *f,void *out,size_t n){return fread(out,1,n,f)==n;}
static bool load_model(Model *m,const char *name){
    FILE *f=fopen(name,"rb");if(!f)return false;
    u32 h[6];bool ok=false;unsigned char *payload=NULL;
    if(!take(f,h,sizeof(h))||memcmp(h,"P2M1",4)||h[1]>16*1024*1024||h[3]>16||h[4]>16)goto done;
    payload=malloc(h[1]);if(!payload||!take(f,payload,h[1])||fgetc(f)!=EOF||crc32(payload,h[1])!=h[2])goto done;
    size_t p=0;u32 vertices=0;
    for(unsigned i=0;i<h[3];i++){
        u32 t[4];if(p+16>h[1])goto done;memcpy(t,payload+p,16);p+=16;
        if(t[0]<8||t[1]<8||t[0]>1024||t[1]>1024||(t[0]&(t[0]-1))||(t[1]&(t[1]-1))||t[2]>2||t[3]>2)goto done;
        size_t size=t[0]*t[1]*4;if(p+size>h[1]||!C3D_TexInit(&m->tex[i],t[0],t[1],GPU_RGBA8))goto done;
        m->nt++;C3D_TexUpload(&m->tex[i],payload+p);p+=size;
        C3D_TexSetFilter(&m->tex[i],GPU_LINEAR,GPU_LINEAR);
        GPU_TEXTURE_WRAP_PARAM wrap[]={GPU_CLAMP_TO_EDGE,GPU_REPEAT,GPU_MIRRORED_REPEAT};
        C3D_TexSetWrap(&m->tex[i],wrap[t[2]],wrap[t[3]]);
    }
    for(unsigned i=0;i<h[4];i++){
        int ti;u32 n;if(p+8>h[1])goto done;memcpy(&ti,payload+p,4);memcpy(&n,payload+p+4,4);p+=8;
        if(ti< -1||ti>=(int)m->nt||!n||n>150000||n%3||p+(size_t)n*sizeof(Vertex)>h[1])goto done;
        Batch *b=&m->batch[i];b->count=n;b->texture=ti;b->v=linearMemAlign(n*sizeof(Vertex),128);if(!b->v)goto done;m->nb++;
        memcpy(b->v,payload+p,n*sizeof(Vertex));p+=n*sizeof(Vertex);vertices+=n;
        for(unsigned j=0;j<n*12;j++)if(!isfinite(((float*)b->v)[j]))goto done;
        if(R_FAILED(GSPGPU_FlushDataCache(b->v,n*sizeof(Vertex))))goto done;
    }
    ok=p==h[1]&&vertices==h[5];
done:free(payload);fclose(f);return ok;
}
static Vertex vertex(float x,float y,float z,float u,float v){return (Vertex){{x,y,z,1},{1,1,1,1},{u,v,0,0}};}
static bool primitives(void){
    plane=linearMemAlign(6*sizeof(Vertex),128);cube=linearMemAlign(36*sizeof(Vertex),128);ring=linearMemAlign(64*6*sizeof(Vertex),128);
    if(!plane||!cube||!ring)return false;
    Vertex q[]={vertex(-1,0,-1,0,0),vertex(1,0,-1,1,0),vertex(1,0,1,1,1),vertex(-1,0,-1,0,0),vertex(1,0,1,1,1),vertex(-1,0,1,0,1)};memcpy(plane,q,sizeof(q));
    const float corners[8][3]={{-1,0,-1},{1,0,-1},{1,2,-1},{-1,2,-1},{-1,0,1},{1,0,1},{1,2,1},{-1,2,1}};
    const int faces[6][4]={{0,1,2,3},{5,4,7,6},{4,0,3,7},{1,5,6,2},{3,2,6,7},{4,5,1,0}},idx[]={0,1,2,0,2,3};
    for(int f=0;f<6;f++)for(int j=0;j<6;j++){const float *p=corners[faces[f][idx[j]]];Vertex v=vertex(p[0],p[1],p[2],j==1||j==2||j==4, j>=2&&j<=4);for(int c=0;c<3;c++)v.c[c]=.65f+.07f*f;cube[f*6+j]=v;}
    for(int i=0;i<64;i++){
        float a=i*6.2831853f/64,b=(i+1)*6.2831853f/64;
        Vertex r[]={vertex(sinf(a),0,cosf(a),0,0),vertex(sinf(b),0,cosf(b),0,0),vertex(.94f*sinf(b),0,.94f*cosf(b),0,0),vertex(sinf(a),0,cosf(a),0,0),vertex(.94f*sinf(b),0,.94f*cosf(b),0,0),vertex(.94f*sinf(a),0,.94f*cosf(a),0,0)};
        memcpy(ring+i*6,r,sizeof(r));
    }
    GSPGPU_FlushDataCache(plane,6*sizeof(Vertex));GSPGPU_FlushDataCache(cube,36*sizeof(Vertex));GSPGPU_FlushDataCache(ring,64*6*sizeof(Vertex));
    u32 pixels[64];for(int i=0;i<64;i++)pixels[i]=~0u;
    if(!C3D_TexInit(&white,8,8,GPU_RGBA8)||!C3D_TexInit(&ground,8,8,GPU_RGBA8))return false;
    C3D_TexUpload(&white,pixels);
    for(int y=0;y<8;y++)for(int x=0;x<8;x++){
        unsigned m=0;for(int b=0;b<3;b++)m|=((x>>b&1)<<(2*b))|((y>>b&1)<<(2*b+1));
        pixels[m]=((x/4+y/4)%2)?0x71963aff:0x8dab52ff;
    }
    C3D_TexUpload(&ground,pixels);C3D_TexSetWrap(&ground,GPU_REPEAT,GPU_REPEAT);C3D_TexSetFilter(&ground,GPU_NEAREST,GPU_NEAREST);
    for(int i=0;i<6;i++){plane[i].uv[0]*=24;plane[i].uv[1]*=24;}GSPGPU_FlushDataCache(plane,6*sizeof(Vertex));return true;
}
static bool initialize(void){
    if(R_FAILED(romfsInit())){error="RomFS unavailable";return false;}
    c3d=C3D_Init(C3D_DEFAULT_CMDBUF_SIZE*2);if(!c3d){error="C3D init";return false;}
    target=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);if(!target)return false;
    C3D_RenderTargetSetOutput(target,GFX_TOP,GFX_LEFT,TRANSFER);
    shader=DVLB_ParseFile((u32*)vshader_shbin,vshader_shbin_size);if(!shader)return false;
    shaderProgramInit(&program);program_ok=R_SUCCEEDED(shaderProgramSetVsh(&program,&shader->DVLE[0]));if(!program_ok)return false;
    C3D_BindProgram(&program);u_mvp=shaderInstanceGetUniformLocation(program.vertexShader,"projection");u_tint=shaderInstanceGetUniformLocation(program.vertexShader,"tint");
    if(u_mvp<0||u_tint<0)return false;
    if(!load_model(&olimar,"romfs:/olimar.p2m")||!load_model(&pikmin,"romfs:/red.p2m")||!load_model(&leaf,"romfs:/leaf.p2m")||!load_model(&onion,"romfs:/onion.p2m")){error="Model validation/load failed";return false;}
    if(!primitives()){error="Primitive allocation";return false;}
    Mtx_PerspTilt(&projection,C3D_AngleFromDegrees(48),C3D_AspectRatioTop,2,1200,false);
    C3D_AttrInfo *a=C3D_GetAttrInfo();AttrInfo_Init(a);for(int i=0;i<3;i++)if(AttrInfo_AddLoader(a,i,GPU_FLOAT,4)<0)return false;
    C3D_TexEnv *e=C3D_GetTexEnv(0);C3D_TexEnvInit(e);C3D_TexEnvSrc(e,C3D_Both,GPU_TEXTURE0,GPU_PRIMARY_COLOR,0);C3D_TexEnvFunc(e,C3D_Both,GPU_MODULATE);
    C3D_CullFace(GPU_CULL_NONE);C3D_DepthTest(true,GPU_GEQUAL,GPU_WRITE_ALL);
    C3D_AlphaTest(true,GPU_GREATER,16);C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);
    return true;
}
static void matrix(V3 p,float angle,V3 scale){
    C3D_Mtx model,mv,mvp;Mtx_Identity(&model);Mtx_Translate(&model,p.x,p.y,p.z,true);Mtx_RotateY(&model,angle,true);Mtx_Scale(&model,scale.x,scale.y,scale.z);
    Mtx_Multiply(&mv,&view,&model);Mtx_Multiply(&mvp,&projection,&mv);C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,u_mvp,&mvp);
}
static void draw(Vertex *v,unsigned n,C3D_Tex *tex,float r,float g,float b,float a){
    C3D_FVUnifSet(GPU_VERTEX_SHADER,u_tint,r,g,b,a);C3D_TexBind(0,tex);
    C3D_BufInfo *buf=C3D_GetBufInfo();BufInfo_Init(buf);if(BufInfo_Add(buf,v,sizeof(Vertex),3,0x210)<0){error="VBO setup";return;}C3D_DrawArrays(GPU_TRIANGLES,0,n);
}
static void model(Model *m,V3 p,float angle,float scale){
    matrix(p,angle,(V3){scale,scale,scale});
    for(unsigned i=0;i<m->nb;i++){Batch *b=&m->batch[i];draw(b->v,b->count,b->texture<0?&white:&m->tex[b->texture],1,1,1,1);}
}
static void draw_ring(V3 p,float radius,float r,float g,float b){p.y=.6f;matrix(p,0,(V3){radius,1,radius});draw(ring,64*6,&white,r,g,b,1);}
static void render(const Game *g){
    if(stalled)return;
    if(!C3D_FrameBegin(C3D_FRAME_NONBLOCK)){
        if(!busy_start)busy_start=osGetTime();
        busy_ms=osGetTime()-busy_start;
        if(busy_ms>=2000){stalled=true;error="GPU queue timeout";}return;
    }
    busy_start=0;busy_ms=0;
    C3D_RenderTargetClear(target,C3D_CLEAR_ALL,0x91BBCCFF,0);C3D_FrameDrawOn(target);
    PortVec3 eye={g->captain.x-sinf(g->yaw)*185,135,g->captain.z-cosf(g->yaw)*185},up={0,1,0},at={g->captain.x,8,g->captain.z+25};PortMtx34 original;
    C_MTXLookAt(original,&eye,&up,&at);Mtx_Identity(&view);for(int i=0;i<3;i++)view.r[i]=FVec4_New(original[i][0],original[i][1],original[i][2],original[i][3]);
    matrix((V3){0,-.1f,25},0,(V3){240,1,235});draw(plane,6,&ground,1,1,1,1);
    draw_ring((V3){-150,0,-100},32,.3f,.85f,1);
    model(&onion,(V3){-150,0,-100},0,.65f);
    matrix((V3){100,0,25},0,(V3){25,17.5f,25});draw(cube,36,&white,.45f,.4f,.32f,1);
    matrix(g->crate,0,(V3){12,9,12});draw(cube,36,&ground,g->delivered?.3f:1,.75f,.2f,1);
    model(&olimar,g->captain,g->face,1.2f);
    for(int i=0;i<SQUAD;i++){
        const Piki *p=&g->pikis[i];model(&pikmin,p->pos,p->face,1);
        V3 stem=p->pos;stem.y+=22;model(&leaf,stem,p->face,.65f);
    }
    draw_ring(g->cursor,8,1,.8f,.1f);
    if(g->calling)draw_ring(g->cursor,g->whistle_radius,.2f,1,.9f);
    C3D_FrameEnd(0);frames++;
}
static void free_model(Model *m){for(unsigned i=0;i<m->nb;i++)linearFree(m->batch[i].v);for(unsigned i=0;i<m->nt;i++)C3D_TexDelete(&m->tex[i]);}
int main(void){
    gfxInitDefault();gfxSet3D(false);gfxSetDoubleBuffering(GFX_BOTTOM,false);consoleInit(GFX_BOTTOM,NULL);
    mkdir("sdmc:/3ds",0777);mkdir("sdmc:/3ds/pikmin2-demo",0777);log_file=fopen("sdmc:/3ds/pikmin2-demo/demo.log","a");
    bool ready=initialize();Parameters p={0};FILE *f=fopen("romfs:/params.bin","rb");if(!f||!take(f,&p,sizeof(p))){ready=false;error="Parameters missing";}if(f)fclose(f);
    Game g;game_init(&g,p);bool tour=DEMO_TOUR;u64 previous=osGetTime(),last_log=0;float accum=0;unsigned ticks=0;Input pending={0};
    while(aptMainLoop()){
        hidScanInput();u32 down=hidKeysDown(),held=hidKeysHeld();if(down&KEY_START)break;
        if(down&KEY_SELECT){tour=!tour;game_init(&g,p);}
        circlePosition cp,cs;hidCircleRead(&cp);hidCstickRead(&cs);
        Input in={.x=-cp.dx/156.f,.z=cp.dy/156.f,.camera=((held&KEY_R)?1:0)-((held&KEY_L)?1:0),.aim_turn=((held&KEY_DLEFT)?1:0)-((held&KEY_DRIGHT)?1:0)-cs.dx/156.f,.aim_range=((held&KEY_DUP)?1:0)-((held&KEY_DDOWN)?1:0)+cs.dy/156.f,.throw_pressed=!!(down&KEY_A),.whistle=!!(held&KEY_B),.dismiss=!!(down&KEY_X),.reset=!!(down&KEY_Y)};
        pending.throw_pressed|=in.throw_pressed;pending.dismiss|=in.dismiss;pending.reset|=in.reset;
        u64 now=osGetTime();accum+=fminf((now-previous)/1000.f,.067f);previous=now;
        while(ready&&accum>=1.f/60){
            if(tour)in=game_tour(g.time);else{in.throw_pressed=pending.throw_pressed;in.dismiss=pending.dismiss;in.reset=pending.reset;}
            game_step(&g,in,1.f/60);pending=(Input){0};accum-=1.f/60;
        }
        if(ready)render(&g);
        if(ticks++%15==0){consoleClear();printf("PIKMIN 2 - PLAYABLE DEMO D01\n%s | GPU:%s Frames:%u\n",tour?"AUTO SHOWCASE":"PLAYER CONTROL",stalled?"TIMEOUT":"OK",frames);
            printf("Squad:%u Air:%u Idle:%u Carry:%u/3\n",game_count(&g,FOLLOW),game_count(&g,AIR),game_count(&g,IDLE),game_count(&g,CARRY));
            printf("Throws:%u Landed:%u Recalled:%u\n",g.throws,g.landings,g.recalls);
            printf("DELIVERED:%u  Aim range:%.0f\n",g.delivered,g.aim_distance);
            printf("Circle: move   L/R: camera\nD-pad / C-stick: aim\nA: throw   Hold B: whistle\nX: dismiss  Y: reset\nSELECT: auto tour  START: exit\n\nThrow 3 Pikmin near gold crate.\nThey carry it to the blue ring.\nWhistle at cursor to regroup.\n\nDisc models/textures, static poses.\nNew arena/AI; no combat or saves.\nNew demo hardware: awaiting test.\nError:%.37s\n",error);gfxFlushBuffers();}
        if(log_file&&now-last_log>2000){fprintf(log_file,"D01 t=%.2f frames=%u busy=%u stalled=%d throws=%u land=%u recall=%u carry=%u delivered=%u error=%s\n",g.time,frames,busy_ms,stalled,g.throws,g.landings,g.recalls,game_count(&g,CARRY),g.delivered,error);fflush(log_file);last_log=now;}
        svcSleepThread(1000000);
    }
    if(c3d){u64 start=osGetTime();while(!stalled){if(C3D_FrameBegin(C3D_FRAME_NONBLOCK)){C3D_FrameEnd(0);break;}if(osGetTime()-start>250){stalled=true;break;}svcSleepThread(1000000);}
        if(!stalled){C3D_Fini();free_model(&olimar);free_model(&pikmin);free_model(&leaf);free_model(&onion);if(program_ok)shaderProgramFree(&program);if(shader)DVLB_Free(shader);if(plane)linearFree(plane);if(cube)linearFree(cube);if(ring)linearFree(ring);if(white.data)C3D_TexDelete(&white);if(ground.data)C3D_TexDelete(&ground);}}
    if(log_file){fprintf(log_file,"EXIT stalled=%d frames=%u\n",stalled,frames);fclose(log_file);}romfsExit();if(!stalled)gfxExit();return 0;
}
