#include <3ds.h>
#include <citro3d.h>
#include <math.h>
#include <string.h>
#include "port_gx.h"
#include "vshader_shbin.h"

#define CAPACITY 32768
#define TRANSFER (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
typedef struct { float x,y,z,r,g,b,a; } Vertex;
static Vertex *vertices;
static unsigned used, errors, remaining;
static PortMtx34 view_matrix, current;
static PortVec3 pending, line_start;
static uint32_t start_color;
static bool have_start, position_pending;
static DVLB_s *shader;
static shaderProgram_s program;
static C3D_RenderTarget *target;
static C3D_Mtx projection;
static int projection_location;

static PortVec3 transform(const PortMtx34 m, PortVec3 p)
{
    return (PortVec3){m[0][0]*p.x+m[0][1]*p.y+m[0][2]*p.z+m[0][3],
                     m[1][0]*p.x+m[1][1]*p.y+m[1][2]*p.z+m[1][3],
                     m[2][0]*p.x+m[2][1]*p.y+m[2][2]*p.z+m[2][3]};
}
static Vertex vertex(PortVec3 p, uint32_t c)
{
    return (Vertex){p.x,p.y,p.z,(c>>24)/255.f,((c>>16)&255)/255.f,((c>>8)&255)/255.f,(c&255)/255.f};
}
static void triangle_eye(PortVec3 a, PortVec3 b, PortVec3 c, uint32_t color)
{
    if (used + 3 > CAPACITY) { ++errors; return; }
    vertices[used++] = vertex(a,color);
    vertices[used++] = vertex(b,color);
    vertices[used++] = vertex(c,color);
}
bool port_gfx_init(void)
{
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return false;
    target = C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,GPU_RB_DEPTH24_STENCIL8);
    if (!target) { C3D_Fini(); return false; }
    C3D_RenderTargetSetOutput(target,GFX_TOP,GFX_LEFT,TRANSFER);
    shader = DVLB_ParseFile((u32*)vshader_shbin,vshader_shbin_size);
    if (!shader) { C3D_RenderTargetDelete(target); C3D_Fini(); return false; }
    shaderProgramInit(&program);
    if (R_FAILED(shaderProgramSetVsh(&program,&shader->DVLE[0]))) {
        shaderProgramFree(&program); DVLB_Free(shader); C3D_RenderTargetDelete(target); C3D_Fini(); return false;
    }
    vertices = linearAlloc(CAPACITY * sizeof(Vertex));
    if (!vertices) { shaderProgramFree(&program); DVLB_Free(shader); C3D_RenderTargetDelete(target); C3D_Fini(); return false; }
    projection_location = shaderInstanceGetUniformLocation(program.vertexShader,"projection");
    /* Original C_MTXLookAt is right-handed: visible eye-space Z is negative. */
    Mtx_PerspTilt(&projection,55.f * (3.14159265f / 180.f),400.f/240.f,.1f,100.f,false);
    return true;
}
void port_gfx_shutdown(void)
{
    C3D_FrameSync();
    linearFree(vertices);
    shaderProgramFree(&program); DVLB_Free(shader);
    C3D_RenderTargetDelete(target); C3D_Fini();
}
void port_gfx_begin(const PortMtx34 view)
{
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    used = 0; remaining = 0; have_start = false; position_pending = false;
    memcpy(view_matrix,view,sizeof(view_matrix)); memcpy(current,view,sizeof(current));
}
void port_gfx_end(void)
{
    if (remaining || position_pending || have_start) ++errors;
    C3D_RenderTargetClear(target,C3D_CLEAR_ALL,0x233C36FF,0);
    C3D_FrameDrawOn(target);
    C3D_BindProgram(&program);
    C3D_AttrInfo *a = C3D_GetAttrInfo();
    AttrInfo_Init(a); AttrInfo_AddLoader(a,0,GPU_FLOAT,3); AttrInfo_AddLoader(a,1,GPU_FLOAT,4);
    C3D_BufInfo *b = C3D_GetBufInfo();
    BufInfo_Init(b); BufInfo_Add(b,vertices,sizeof(Vertex),2,0x10);
    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env); C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,0,0); C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthTest(true,GPU_GEQUAL,GPU_WRITE_ALL);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,projection_location,&projection);
    GSPGPU_FlushDataCache(vertices,used*sizeof(Vertex));
    C3D_DrawArrays(GPU_TRIANGLES,0,used);
    C3D_FrameEnd(0);
}
unsigned port_gfx_errors(void) { return errors; }
void port_gfx_triangle(PortVec3 a, PortVec3 b, PortVec3 c, uint32_t color)
{ triangle_eye(transform(view_matrix,a),transform(view_matrix,b),transform(view_matrix,c),color); }
void port_gfx_box(PortVec3 c,float x,float y,float z,uint32_t color)
{
    PortVec3 p[8];
    for (int i=0;i<8;i++) p[i]=(PortVec3){c.x+((i&1)?x:-x),c.y+((i&2)?y:-y),c.z+((i&4)?z:-z)};
    static const unsigned faces[12][3]={{0,1,3},{0,3,2},{4,6,7},{4,7,5},{0,4,5},{0,5,1},{2,3,7},{2,7,6},{0,2,6},{0,6,4},{1,5,7},{1,7,3}};
    for(int i=0;i<12;i++) port_gfx_triangle(p[faces[i][0]],p[faces[i][1]],p[faces[i][2]],color);
}
void P2_GXLoadPosMtxImm(const PortMtx34 matrix,unsigned index)
{ if (index) { ++errors; return; } memcpy(current,matrix,sizeof(current)); }
void P2_GXBegin(int primitive,int format,unsigned count)
{
    if (remaining || have_start || position_pending) ++errors;
    remaining=0; have_start=false; position_pending=false;
    if (primitive!=P2_GX_LINES || format!=P2_GX_VTXFMT0 || (count&1)) { ++errors; return; }
    remaining=count;
}
void P2_GXPosition3f32(float x,float y,float z)
{
    if (!remaining || position_pending) { ++errors; return; }
    pending=transform(current,(PortVec3){x,y,z}); position_pending=true;
}
void P2_GXColor4u8(uint8_t r,uint8_t g,uint8_t b,uint8_t a)
{
    if (!position_pending || !remaining) { ++errors; return; }
    position_pending=false; --remaining;
    uint32_t color=((uint32_t)r<<24)|((uint32_t)g<<16)|((uint32_t)b<<8)|a;
    if (!have_start) { line_start=pending; start_color=color; have_start=true; return; }
    if (color!=start_color) ++errors; /* Gradients not implemented by this first GX path. */
    PortVec3 delta={pending.x-line_start.x,pending.y-line_start.y,0};
    float length=sqrtf(delta.x*delta.x+delta.y*delta.y);
    if (length>1.e-6f) {
        float dx=-delta.y/length*.012f, dy=delta.x/length*.012f;
        PortVec3 p={line_start.x+dx,line_start.y+dy,line_start.z};
        PortVec3 q={line_start.x-dx,line_start.y-dy,line_start.z};
        PortVec3 s={pending.x+dx,pending.y+dy,pending.z};
        PortVec3 t={pending.x-dx,pending.y-dy,pending.z};
        triangle_eye(p,q,s,color); triangle_eye(q,t,s,color);
    }
    have_start=false;
}
