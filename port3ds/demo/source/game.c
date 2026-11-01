/* Native demo controller. Original equations adapted from Navi::throwPiki,
 * FakePiki::{moveVelocity,moveRotation}, Piki::getSpeed. The small formation,
 * collision arena, crate and state machine below are NEW demo code, not the
 * original Pikmin AI, CPlate, map collision or treasure transport system. */
#include "game.h"
#include "port_math.h"
#include <math.h>
#include <string.h>
static float length(float x,float z){return sqrtf(x*x+z*z);}
static float clamp(float v,float a,float b){return fminf(b,fmaxf(a,v));}
static float distance(V3 a,V3 b){return length(a.x-b.x,a.z-b.z);}
static V3 base={-150,0,-100};
unsigned game_count(const Game *g,PikiState s){unsigned n=0;for(int i=0;i<SQUAD;i++)n+=g->pikis[i].state==s;return n;}
void game_init(Game *g,Parameters p){
    memset(g,0,sizeof(*g));g->parms=p;g->aim_distance=110;
    g->crate=(V3){0,0,125};
    for(int i=0;i<SQUAD;i++){g->pikis[i].pos=(V3){(i-2)*12.f,0,-25-(i%2)*12.f};}
}
static void collide(V3 *p,float radius){
    p->x=clamp(p->x,-240+radius,240-radius);p->z=clamp(p->z,-210+radius,260-radius);
    /* One authored solid block. Resolve using smallest horizontal penetration. */
    float dx=p->x-100,dz=p->z-25,extent=25+radius;
    if(p->y<35 && fabsf(dx)<extent && fabsf(dz)<extent){
        if(fabsf(dx)>fabsf(dz))p->x=100+copysignf(extent,dx);else p->z=25+copysignf(extent,dz);
    }
}
static void walk(Piki *p,V3 target,float speed,float dt){
    float dx=target.x-p->pos.x,dz=target.z-p->pos.z,d=length(dx,dz);
    float s=fminf(speed,d/dt);
    p->vel.x=d>1?dx/d*s:0;p->vel.z=d>1?dz/d*s:0;
    p->pos.x+=p->vel.x*dt;p->pos.z+=p->vel.z*dt;
    if(d>1)p->face=roundAng(atan2f(dx,dz));
    collide(&p->pos,4);
}
void game_step(Game *g,Input in,float dt){
    if(in.reset){Parameters p=g->parms;game_init(g,p);return;}
    if(!(dt>0 && dt<=.05f))return;
    g->time+=dt;g->steps++;
    g->yaw=roundAng(g->yaw+in.camera*dt*1.6f);
    g->aim_angle=clamp(g->aim_angle+in.aim_turn*dt*2,-3.14f,3.14f);
    g->aim_distance=clamp(g->aim_distance+in.aim_range*dt*80,20,170);
    float mag=length(in.x,in.z),x=0,z=0;
    if(mag>.1f){
        /* Circular normalisation and 10 degree input bins from Navi controller.
         * Native demo omits original low-stick cursor-only movement mode. */
        float a=roundAng(atan2f(in.x,in.z)),bin=3.141592654f/18.f;
        a=bin*(int)((bin*.5f+a)/bin);mag=fminf(1,mag);
        x=sinf(a+g->yaw)*mag*g->parms.move;z=cosf(a+g->yaw)*mag*g->parms.move;
        float target=roundAng(atan2f(x,z));
        g->face=roundAng(g->face+8*dt*angDist(target,g->face));
    }
    /* FakePiki ground velocity response, flat floor, no slip/platform forces. */
    g->velocity.x+=(x-g->velocity.x)*(dt/.1f);g->velocity.z+=(z-g->velocity.z)*(dt/.1f);
    g->captain.x+=g->velocity.x*dt;g->captain.z+=g->velocity.z*dt;collide(&g->captain,8.5f);
    float aim=g->face+g->aim_angle;
    g->cursor=(V3){g->captain.x+sinf(aim)*g->aim_distance,0,g->captain.z+cosf(aim)*g->aim_distance};
    if(in.dismiss)for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==FOLLOW)g->pikis[i].state=IDLE;
    if(in.throw_pressed){
        for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==FOLLOW && distance(g->pikis[i].pos,g->captain)<85){
            Piki *p=&g->pikis[i];
            p->state=AIR;p->pos=(V3){g->captain.x-15*sinf(g->face),10,g->captain.z-15*cosf(g->face)};
            float t=.5f*g->parms.landing_time;
            float vy=t*(.5f*g->parms.gravity)+g->parms.throw_height/t;
            float flight=2*vy/g->parms.gravity;
            p->vel=(V3){(g->cursor.x-p->pos.x)/flight+g->velocity.x,vy,(g->cursor.z-p->pos.z)/flight+g->velocity.z};
            g->throws++;break;
        }
    }
    g->calling=in.whistle;
    if(in.whistle){
        g->whistle_time=fminf(g->parms.call_time,g->whistle_time+dt);
        g->whistle_radius=g->parms.call_min+(g->parms.call_max-g->parms.call_min)*g->whistle_time/g->parms.call_time;
        for(int i=0;i<SQUAD;i++){
            Piki *p=&g->pikis[i];
            if(p->state!=AIR && p->state!=FOLLOW && distance(p->pos,g->cursor)<=g->whistle_radius){p->state=FOLLOW;g->recalls++;}
        }
    }else{g->whistle_time=0;g->whistle_radius=0;}
    for(int i=0;i<SQUAD;i++){
        Piki *p=&g->pikis[i];
        if(p->state==AIR){
            p->vel.y-=g->parms.gravity*dt;
            p->pos.x+=p->vel.x*dt;p->pos.y+=p->vel.y*dt;p->pos.z+=p->vel.z*dt;
            collide(&p->pos,4);
            if(p->pos.y<=0){p->pos.y=0;p->vel=(V3){0,0,0};p->state=IDLE;g->landings++;}
        }else if(p->state==FOLLOW){
            float side=(i%3-1)*13.f,behind=25+(i/3)*16.f;
            V3 target={g->captain.x+cosf(g->face)*side-sinf(g->face)*behind,0,g->captain.z-sinf(g->face)*side-cosf(g->face)*behind};
            /* Piki::getSpeed(multiplier): leaf speed interpolation 10..120. */
            float mult=clamp(distance(p->pos,target)/45,0,1);
            walk(p,target,mult*(g->parms.run-g->parms.walk)+g->parms.walk,dt);
        }else if(p->state==IDLE && !g->delivered && distance(p->pos,g->crate)<35){p->state=CARRY;}
    }
    unsigned carriers=game_count(g,CARRY);
    if(carriers>=3 && !g->delivered){
        float d=distance(g->crate,base);
        if(d<7){g->delivered=1;for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==CARRY)g->pikis[i].state=IDLE;}
        else{g->crate.x+=(base.x-g->crate.x)/d*28*dt;g->crate.z+=(base.z-g->crate.z)/d*28*dt;}
    }
    for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==CARRY){
        float a=i*6.2831853f/SQUAD;
        g->pikis[i].pos=(V3){g->crate.x+18*sinf(a),0,g->crate.z+18*cosf(a)};
    }
}
Input game_tour(float t){
    /* Optional unattended showcase, visible on HUD; same game_step as controls. */
    Input in={0};
    if(t<.5f)in.aim_range=0;
    if((t>=1&&t<1.02f)||(t>=2&&t<2.02f)||(t>=3&&t<3.02f))in.throw_pressed=true;
    if(t>5&&t<8)in.x=-.6f;
    if(t>8&&t<11)in.z=-.5f;
    if(t>12&&t<16)in.camera=.5f;
    if(t>17&&t<18)in.dismiss=true;
    if(t>18) {in.whistle=true;in.aim_range=-1;}
    return in;
}
