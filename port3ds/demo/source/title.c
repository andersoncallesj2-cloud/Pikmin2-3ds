/* Original title coordinates/parameters; native arrival steering and input.
 * Full ebi boids, seasonal state machine and title enemies are not ported. */
#include "title.h"
#include <3ds.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
TitleUnit title_units[500];V3 title_camera;float title_fov,title_scale[2],title_time;unsigned title_logo;
static V3 goals[2][500];static unsigned counts[2][5];
bool title_init(void){
    FILE *f=fopen("romfs:/title-scene.bin","rb");if(!f)return false;char magic[4];float v[6];bool ok=false;
    if(fread(magic,1,4,f)!=4||memcmp(magic,"P2L1",4)||fread(v,4,6,f)!=6)goto done;
    for(unsigned i=0;i<6;i++)if(!isfinite(v[i]))goto done;
    title_camera=(V3){v[0],v[1],v[2]};title_fov=v[3];title_scale[0]=v[4];title_scale[1]=v[5];
    if(title_fov<=0||title_fov>=170||title_scale[0]<=0||title_scale[1]<=0)goto done;
    for(unsigned logo=0;logo<2;logo++)for(unsigned color=0;color<5;color++){
        unsigned n;if(fread(&n,4,1,f)!=1||n>100)goto done;counts[logo][color]=n;
        for(unsigned i=0;i<n;i++){float xy[2];if(fread(xy,4,2,f)!=2||!isfinite(xy[0])||!isfinite(xy[1]))goto done;goals[logo][color*100+i]=(V3){xy[0],0,-xy[1]};}
    }
    for(unsigned i=0;i<500;i++){float a=i*2.39996323f;title_units[i]=(TitleUnit){.pos={550*sinf(a),0,550*cosf(a)},.color=i/100};}
    ok=fgetc(f)==EOF;
done:fclose(f);return ok;
}
void title_step(float dt,uint32_t down){
    title_time+=dt;if(down&KEY_L)title_logo=0;if(down&KEY_R)title_logo=1;
    for(unsigned i=0;i<500;i++){
        TitleUnit *u=&title_units[i];u->active=i%100<counts[title_logo][i/100];if(!u->active)continue;
        V3 target=goals[title_logo][i];float dx=target.x-u->pos.x,dz=target.z-u->pos.z,d=sqrtf(dx*dx+dz*dz);
        float step=fminf(d,150*dt);if(d>.01f){u->pos.x+=dx/d*step;u->pos.z+=dz/d*step;u->face=atan2f(dx,dz);}
    }
}
