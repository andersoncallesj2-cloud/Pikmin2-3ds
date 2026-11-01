/* Original collision vertices/triangles; new native broadphase and floor query. */
#include "stage.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
static void *data;static float *tri;static uint32_t *offsets,*refs,nx,nz,nt,nref;static float step,minx,minz;
static unsigned selected,waters;static float water[16][6];
static float landmarks[5][4],storage_points[9][3];static bool storage_valid;
bool stage_landmark(unsigned id,float out[4]){if(!selected||!storage_valid||id>=5)return false;memcpy(out,landmarks[id],16);return true;}
bool stage_storage_point(unsigned id,float out[3]){if(!storage_valid||id>=9)return false;memcpy(out,storage_points[id],12);return true;}
bool stage_init(void){
    FILE *f=fopen("romfs:/valley.p2c","rb");if(!f)return false;
    struct {char magic[4];uint32_t nt,nx,nz,nrefs;float step,x,z;uint32_t crc;} h;
    bool ok=false;if(fread(&h,1,sizeof(h),f)!=sizeof(h)||memcmp(h.magic,"P2C1",4)||h.nt>20000||!h.nx||!h.nz||h.nx*h.nz>30000||h.nrefs>1000000||h.step<=0)goto done;
    size_t bytes=h.nt*36+(h.nx*h.nz+1+h.nrefs)*4;data=malloc(bytes);if(!data||fread(data,1,bytes,f)!=bytes)goto done;
    nt=h.nt;nx=h.nx;nz=h.nz;nref=h.nrefs;step=h.step;minx=h.x;minz=h.z;tri=data;offsets=(uint32_t*)(tri+nt*9);refs=offsets+nx*nz+1;
    for(unsigned i=0;i<=nx*nz;i++)if(offsets[i]>nref||(i&&offsets[i]<offsets[i-1]))goto done;
    for(unsigned i=0;i<nref;i++)if(refs[i]>=nt)goto done;
    ok=true;
done:fclose(f);if(!ok){free(data);data=NULL;return false;}
    f=fopen("romfs:/valley-water.bin","rb");if(f){if(fread(&waters,4,1,f)!=1||waters>16)waters=0;else if(fread(water,24,waters,f)!=waters)waters=0;fclose(f);}
    f=fopen("romfs:/storage.bin","rb");if(!f)return false;char magic[4];float values[47];bool valid=fread(magic,1,4,f)==4&&!memcmp(magic,"P2N1",4)&&fread(values,4,47,f)==47&&fgetc(f)==EOF;fclose(f);
    for(unsigned i=0;i<47;i++)if(!isfinite(values[i]))valid=false;
    if(!valid)return false;
    memcpy(landmarks,values,sizeof(landmarks));memcpy(storage_points,values+20,sizeof(storage_points));storage_valid=true;return true;
}
void stage_select(unsigned s){selected=s&&data?1:0;}
float stage_height(float x,float z,float hint){
    if(!selected||!data)return 0;
    int cx=(int)floorf((x-minx)/step),cz=(int)floorf((z-minz)/step);if(cx<0||cz<0||cx>=(int)nx||cz>=(int)nz)return hint;
    unsigned cell=cz*nx+cx;float best=-100000,above=100000;
    for(unsigned i=offsets[cell];i<offsets[cell+1];i++){
        float *t=tri+refs[i]*9;float ax=t[3]-t[0],az=t[5]-t[2],bx=t[6]-t[0],bz=t[8]-t[2],det=ax*bz-az*bx;if(fabsf(det)<.01f)continue;
        float u=((x-t[0])*bz-(z-t[2])*bx)/det,v=(ax*(z-t[2])-az*(x-t[0]))/det;if(u<-.001f||v<-.001f||u+v>1.001f)continue;
        float y=t[1]+u*(t[4]-t[1])+v*(t[7]-t[1]);
        if(y<=hint+45&&y>best)best=y;else if(y<above)above=y;
    }
    if(best>-99999)return best;
    return above<100000&&above<hint+60?above:hint;
}
bool stage_water(float x,float y,float z){
    if(!selected)return x>100&&x<380&&z>175&&z<425;
    for(unsigned i=0;i<waters;i++)if(x>water[i][0]&&x<water[i][3]&&z>water[i][2]&&z<water[i][5]&&y<water[i][4])return true;
    return false;
}
void stage_exit(void){free(data);data=NULL;}
