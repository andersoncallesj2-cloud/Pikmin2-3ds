#include "model.h"
#include "port_gx.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int draws;
void port_gfx_triangle(PortVec3 a,PortVec3 b,PortVec3 c,uint32_t color)
{ (void)b; (void)c; assert(a.z==3); assert(color==0xdcdcdcff); ++draws; }
int main(int argc,char **argv)
{
    assert(argc==2);
    assert(!model_load("/definitely/missing.p2mesh"));
    assert(model_load(argv[1]) && model_triangles()==1);
    model_draw(); assert(draws==1);
    /* Corrupting a byte must fail CRC and release the previous loaded model. */
    FILE *f=fopen(argv[1],"r+b"); assert(f);
    fseek(f,24,SEEK_SET); fputc(0xff,f); fclose(f);
    assert(!model_load(argv[1]) && model_triangles()==0);
    assert(strstr(model_status(),"corrupt"));
    model_free(); puts("Mesh loader: valid draw, missing file and CRC rejection pass.");
}
