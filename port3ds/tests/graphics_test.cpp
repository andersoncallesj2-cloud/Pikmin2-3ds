#include "port_gx.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
static PortMtx34 current;
static unsigned matrices,primitives,positions,colors;
static PortVec3 pending,center;
static float radius;
extern "C" void P2_GXLoadPosMtxImm(const PortMtx34 m,unsigned index)
{ assert(index==0); memcpy(current,m,sizeof(current)); ++matrices; }
extern "C" void P2_GXBegin(int primitive,int format,unsigned count)
{ assert(primitive==P2_GX_LINES && format==P2_GX_VTXFMT0 && count==2); ++primitives; }
extern "C" void P2_GXPosition3f32(float x,float y,float z)
{
    pending={current[0][0]*x+current[0][1]*y+current[0][2]*z+current[0][3],
             current[1][0]*x+current[1][1]*y+current[1][2]*z+current[1][3],
             current[2][0]*x+current[2][1]*y+current[2][2]*z+current[2][3]};
    float dx=pending.x-center.x,dy=pending.y-center.y,dz=pending.z-center.z;
    float r=std::sqrt(dx*dx+dy*dy+dz*dz);
    assert(std::isfinite(r) && std::fabs(r-radius)<.0001f); ++positions;
}
extern "C" void P2_GXColor4u8(uint8_t r,uint8_t g,uint8_t b,uint8_t a)
{ assert(r==0x12 && g==0x34 && b==0x56 && a==0xff); ++colors; }
int main()
{
    PortMtx34 view={{1,0,0,0},{0,1,0,0},{0,0,1,0}};
    center={2,3,-4}; radius=.75f;
    port_graphics_sphere(view,center,radius,0x123456ff);
    assert(matrices==32 && primitives==512 && positions==1024 && colors==1024);
    printf("Original Graphics sphere: 512 lines, 1024 transformed vertices satisfy radius/color/count invariants.\n");
}
