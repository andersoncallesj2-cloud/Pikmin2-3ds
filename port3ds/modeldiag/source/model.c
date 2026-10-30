#include "model.h"
#include "port_gx.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x,y,z; uint8_t r,g,b,a; } ModelVertex;
_Static_assert(sizeof(ModelVertex)==16,"Mesh disk layout changed");
static ModelVertex *data;
static unsigned count;
static float angle;
static const char *status="not loaded";
static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint32_t crc32_bytes(const uint8_t *p,size_t size)
{
    uint32_t c=~0u;
    for(size_t i=0;i<size;i++) { c^=p[i]; for(int j=0;j<8;j++)c=(c>>1)^(0xedb88320u&-(c&1)); }
    return ~c;
}
void model_free(void) { free(data); data=NULL; count=0; }
bool model_load(const char *path)
{
    model_free();
    FILE *f=fopen(path,"rb");
    if(!f) { status="no model installed"; return false; }
    uint8_t h[24]; ModelVertex *next=NULL;
    status="invalid mesh header";
    if(fread(h,1,sizeof(h),f)!=sizeof(h) || memcmp(h,"P2M1",4) || le32(h+4)!=1 || le32(h+12)!=16)goto done;
    unsigned n=le32(h+8);
    if(n<3 || n>18000 || n%3 || le32(h+16)!=n*16)goto done;
    status="model allocation failed";
    next=malloc(n*sizeof(*next)); if(!next)goto done;
    status="truncated/corrupt mesh";
    if(fread(next,sizeof(*next),n,f)!=n || fgetc(f)!=EOF || crc32_bytes((const uint8_t*)next,n*16)!=le32(h+20))goto done;
    status="non-finite/outside bounds";
    for(unsigned i=0;i<n;i++)if(!isfinite(next[i].x)||!isfinite(next[i].y)||!isfinite(next[i].z)||
        fabsf(next[i].x)>20||fabsf(next[i].y)>20||fabsf(next[i].z)>20)goto done;
    data=next; next=NULL; count=n; status="static mesh loaded";
done:
    free(next); fclose(f); return data!=NULL;
}
void model_draw(void)
{
    float sine=sinf(angle),cosine=cosf(angle);
    for(unsigned i=0;i<count;i+=3) {
        ModelVertex a=data[i],b=data[i+1],c=data[i+2];
        uint32_t rgba=(uint32_t)a.r<<24|(uint32_t)a.g<<16|(uint32_t)a.b<<8|a.a;
        port_gfx_triangle((PortVec3){a.x*cosine+a.z*sine,a.y,-a.x*sine+a.z*cosine+3},
                          (PortVec3){b.x*cosine+b.z*sine,b.y,-b.x*sine+b.z*cosine+3},
                          (PortVec3){c.x*cosine+c.z*sine,c.y,-c.x*sine+c.z*cosine+3},rgba);
    }
}
void model_turn(void) { angle+=1.5707963f; if(angle>=6.2831852f)angle=0; }
const char *model_status(void) { return status; }
unsigned model_triangles(void) { return count/3; }
