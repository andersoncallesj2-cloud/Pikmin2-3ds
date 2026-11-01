/* Native C adapters of Navi::{reviseController,makeVelocity,throwPiki},
 * NaviWhistle::update, Piki::getSpeed, FakePiki movement, and ActFormation's
 * settling branches. Reference: CC0 projectPiki 999140f3521e22619547d5d61862a6577ea73882.
 * Arena routing, combat and item FSM are NEW simplified demo implementations. */
#include "game.h"
#include "port_math.h"
#include <math.h>
#include <string.h>
#define PI 3.141592654f
static const V3 onion_home={-150,0,-100},ship_home={145,0,-120};
static float length(float x,float z){return sqrtf(x*x+z*z);}
static float distance(V3 a,V3 b){return length(a.x-b.x,a.z-b.z);}
static float clamp(float x,float a,float b){return fmaxf(a,fminf(b,x));}
unsigned game_count(const Game *g,PikiState s){unsigned n=0;for(int i=0;i<SQUAD;i++)n+=g->pikis[i].state==s;return n;}
static unsigned assigned(const Game *g,int item){unsigned n=0;for(int i=0;i<SQUAD;i++)n+=g->pikis[i].state==CARRY&&g->pikis[i].item==item;return n;}
unsigned game_carriers(const Game *g,int item){unsigned n=0;for(int i=0;i<SQUAD;i++)n+=g->pikis[i].state==CARRY&&g->pikis[i].item==item&&distance(g->pikis[i].pos,g->items[item].pos)<65;return n;}
void game_init(Game *g,Parameters p){
    memset(g,0,sizeof(*g));g->parms=p;g->held_piki=-1;
    g->cursor_offset=(V3){0,0,p.cursor_radius*.5f};g->cursor=g->cursor_offset;
    g->other_captain=(V3){25,0,-5};g->other_cursor=g->cursor_offset;
    g->captain_health[0]=g->captain_health[1]=p.captain_max_health;
    g->items[0]=(CarryItem){.pos={0,0,85},.weight=(int)p.treasure_weight,.max_carriers=(int)p.treasure_max};
    g->items[1]=(CarryItem){.weight=(int)p.carcass_weight,.max_carriers=(int)p.carcass_max};
    g->crate=g->items[0].pos;g->population=SQUAD;
    g->enemy=(Bulborb){.pos={-110,0,160},.face=PI,.health=p.enemy_health};
    for(int i=0;i<SQUAD;i++)g->pikis[i]=(Piki){.pos={(i%5-2)*11.f,0,-25-(i/5)*12.f},.item=-1};
    play_camera_init(g);
}
static void collide(V3 *p,float r){
    p->x=clamp(p->x,-240+r,240-r);p->z=clamp(p->z,-210+r,260-r);
    float dx=p->x-100,dz=p->z-25,e=25+r;
    if(p->y<35&&fabsf(dx)<e&&fabsf(dz)<e){if(fabsf(dx)>fabsf(dz))p->x=100+copysignf(e,dx);else p->z=25+copysignf(e,dz);}
}
static bool blocked(V3 a,V3 b,float r){
    float e=25+r,lo=0,hi=1,start[]={a.x-100,a.z-25},delta[]={b.x-a.x,b.z-a.z};
    for(int i=0;i<2;i++){
        if(fabsf(delta[i])<.0001f){if(fabsf(start[i])>=e)return false;}
        else{float t0=(-e-start[i])/delta[i],t1=(e-start[i])/delta[i];if(t0>t1){float t=t0;t0=t1;t1=t;}lo=fmaxf(lo,t0);hi=fminf(hi,t1);if(lo>=hi)return false;}
    }
    return hi>0&&lo<1;
}
static V3 waypoint(V3 from,V3 to,float r){
    if(!blocked(from,to,r))return to;
    /* A new visibility graph around this arena's single block. */
    float e=26+r;V3 nodes[]={from,to,{100-e,0,25-e},{100+e,0,25-e},{100+e,0,25+e},{100-e,0,25+e}};
    float cost[6]={0,1e9f,1e9f,1e9f,1e9f,1e9f};int prev[6]={-1,-1,-1,-1,-1,-1};bool done[6]={0};
    for(int step=0;step<6;step++){
        int u=-1;for(int i=0;i<6;i++)if(!done[i]&&(u<0||cost[i]<cost[u]))u=i;
        if(u<0||cost[u]>=1e9f)break;
        done[u]=true;
        for(int v=0;v<6;v++)if(!done[v]&&!blocked(nodes[u],nodes[v],r)){
            float c=cost[u]+distance(nodes[u],nodes[v]);if(c<cost[v]){cost[v]=c;prev[v]=u;}
        }
    }
    if(prev[1]<0)return to;
    int at=1;while(prev[at]>0)at=prev[at];return nodes[at];
}
static void walk(Piki *p,V3 goal,float speed,float dt){
    V3 t=waypoint(p->pos,goal,4);float dx=t.x-p->pos.x,dz=t.z-p->pos.z,d=length(dx,dz);
    speed=fminf(speed,d/dt);p->vel.x=d>.2f?dx/d*speed:0;p->vel.z=d>.2f?dz/d*speed:0;
    p->pos.x+=p->vel.x*dt;p->pos.z+=p->vel.z*dt;
    if(d>1)p->face=roundAng(p->face+fminf(1,dt*8)*angDist(roundAng(atan2f(dx,dz)),p->face));
    collide(&p->pos,4);
}
static void pick_work(Game *g,Piki *p){
    if(!g->enemy.dead&&distance(p->pos,g->enemy.pos)<55){p->state=ATTACK;p->work_time=0;return;}
    for(int n=0;n<2;n++)if((n==0||g->enemy.dead)&&!g->items[n].delivered&&distance(p->pos,g->items[n].pos)<60&&assigned(g,n)<(unsigned)g->items[n].max_carriers){p->state=CARRY;p->item=n;return;}
}
static void throw_piki(Game *g){
    if(g->held_piki<0)return;
    Piki *p=&g->pikis[g->held_piki];p->state=AIR;
    p->pos=(V3){g->captain.x-15*sinf(g->face),10,g->captain.z-15*cosf(g->face)};
    float half=.5f*g->parms.landing_time,vy=half*(.5f*g->parms.gravity)+g->parms.throw_height/half,time=2*vy/g->parms.gravity;
    p->vel=(V3){(g->cursor.x-p->pos.x)/time+g->velocity.x,vy,(g->cursor.z-p->pos.z)/time+g->velocity.z};
    g->throws++;g->held_piki=-1;
}
static void enemy_step(Game *g,float dt){
    Bulborb *e=&g->enemy;if(e->dead)return;
    if(e->health<=0){
        e->dead=true;e->state=3;g->items[1].pos=e->pos;
        for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==ATTACK){g->pikis[i].state=IDLE;g->pikis[i].pos.y=0;pick_work(g,&g->pikis[i]);}
        return;
    }
    e->cooldown=fmaxf(0,e->cooldown-dt);V3 target=g->captain;float nearest=distance(e->pos,target);
    for(int i=0;i<SQUAD;i++)if(g->pikis[i].state!=LOST&&g->pikis[i].state!=ATTACK&&g->pikis[i].pos.y<25){float d=distance(e->pos,g->pikis[i].pos);if(d<nearest){nearest=d;target=g->pikis[i].pos;}}
    if(e->state==0&&nearest>g->parms.enemy_wake)return;
    e->state=1;float angle=roundAng(atan2f(target.x-e->pos.x,target.z-e->pos.z));
    e->face=roundAng(e->face+fminf(1,dt*2)*angDist(angle,e->face));
    if(nearest>g->parms.enemy_attack_radius*.75f){
        V3 next=waypoint(e->pos,target,22);float d=distance(e->pos,next);
        if(d>1){e->pos.x+=(next.x-e->pos.x)/d*g->parms.enemy_speed*dt;e->pos.z+=(next.z-e->pos.z)/d*g->parms.enemy_speed*dt;collide(&e->pos,22);}
    }else if(e->cooldown<=0){
        e->state=2;e->cooldown=1.8f;int eaten=0;
        for(int i=0;i<SQUAD&&eaten<2;i++){
            Piki *p=&g->pikis[i];float a=atan2f(p->pos.x-e->pos.x,p->pos.z-e->pos.z);
            if(p->state!=LOST&&p->state!=HELD&&p->state!=ATTACK&&p->pos.y<25&&distance(p->pos,e->pos)<g->parms.enemy_attack_radius&&cosf(a-e->face)>.2f){p->state=LOST;p->item=-1;g->deaths++;eaten++;}
        }
        if(distance(g->captain,e->pos)<g->parms.enemy_attack_radius)g->captain_health[g->active_captain]=fmaxf(0,g->captain_health[g->active_captain]-g->parms.enemy_damage);
    }
}
void game_step(Game *g,Input in,float dt){
    if(in.reset){Parameters p=g->parms;game_init(g,p);return;}
    if(!(dt>0&&dt<=.05f))return;
    g->time+=dt;g->steps++;
    if(in.switch_pressed&&g->held_piki<0&&g->captain_health[g->active_captain^1]>0){
        V3 p=g->captain;g->captain=g->other_captain;g->other_captain=p;p=g->cursor_offset;g->cursor_offset=g->other_cursor;g->other_cursor=p;
        float f=g->face;g->face=g->other_face;g->other_face=f;g->velocity=(V3){0};g->active_captain^=1;g->camera.follow_time=g->parms.camera[g->active_captain].follow;
        for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==FOLLOW)g->pikis[i].state=IDLE;
    }
    if(g->captain_health[g->active_captain]<=0)in.x=in.z=0;
    float mag=length(in.x,in.z),x=0,z=0;
    if(mag>0){
        /* Navi::reviseController, including original octagonal correction. */
        float a=roundAng(atan2f(in.x,in.z)),bin=g->parms.shake*PI/180;
        a=bin*(int)((bin*.5f+a)/bin);float boost=a-(int)(a/(PI/4))*(PI/4);
        mag*=(sinf(boost)+sinf(PI/4-boost))/sinf(PI/4);
        if(mag>=g->parms.stick_clamp)mag=1;
        if(mag<g->parms.neutral)mag=0;
        x=sinf(a+g->yaw)*mag;z=cosf(a+g->yaw)*mag;
    }
    g->still_time=mag>g->parms.neutral?0:g->still_time+dt;
    float vx=x*g->parms.move,vz=z*g->parms.move;
    if((mag>g->parms.neutral||g->still_time>=g->parms.cursor_look_time)&&mag<=g->parms.cursor_threshold){
        vx=vz=0;float a=roundAng(atan2f(g->cursor_offset.x,g->cursor_offset.z));g->face=roundAng(g->face+(1-powf(.8f,dt*30))*angDist(a,g->face));
    }else if(mag>g->parms.neutral)g->face=roundAng(g->face+8*dt*angDist(roundAng(atan2f(x,z)),g->face));
    g->velocity.x+=(vx-g->velocity.x)*(dt/.1f);g->velocity.z+=(vz-g->velocity.z)*(dt/.1f);
    g->captain.x+=g->velocity.x*dt;g->captain.z+=g->velocity.z*dt;collide(&g->captain,8.5f);
    /* NaviWhistle::update: normalized stick and tangent at the cursor boundary. */
    float d=length(x,z);
    if(d>0){
        float sx=x/d*g->parms.cursor_speed,sz=z/d*g->parms.cursor_speed;V3 next={g->cursor_offset.x+sx*dt,0,g->cursor_offset.z+sz*dt};float r=length(next.x,next.z);
        if(r>=g->parms.cursor_radius){float nx=next.x/r,nz=next.z/r,dot=nx*sx+nz*sz;next=(V3){g->cursor_offset.x+(sx-nx*dot)*dt,0,g->cursor_offset.z+(sz-nz*dot)*dt};}
        g->cursor_offset=next;
    }
    g->cursor=(V3){g->captain.x+g->cursor_offset.x,0,g->captain.z+g->cursor_offset.z};g->aim_distance=length(g->cursor_offset.x,g->cursor_offset.z);
    if(in.dismiss)for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==FOLLOW)g->pikis[i].state=IDLE;
    if(in.grab_pressed&&g->held_piki<0){
        for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==FOLLOW&&distance(g->pikis[i].pos,g->captain)<85){g->held_piki=i;g->pikis[i].state=HELD;break;}
        if(g->held_piki<0&&!g->enemy.dead&&distance(g->enemy.pos,g->captain)<30+g->parms.punch_range)g->enemy.health-=g->parms.punch_damage;
    }
    if(in.throw_pressed||(g->held_piki>=0&&!in.grab_held))throw_piki(g);
    if(g->held_piki>=0)g->pikis[g->held_piki].pos=(V3){g->captain.x+8*sinf(g->face),25,g->captain.z+8*cosf(g->face)};
    g->calling=in.whistle;
    if(in.whistle){
        g->whistle_time=fminf(g->parms.call_time,g->whistle_time+dt);g->whistle_radius=g->parms.call_min+(g->parms.call_max-g->parms.call_min)*g->whistle_time/g->parms.call_time;
        for(int i=0;i<SQUAD;i++){Piki *p=&g->pikis[i];if(p->state!=AIR&&p->state!=FOLLOW&&p->state!=LOST&&p->state!=HELD&&distance(p->pos,g->cursor)<=g->whistle_radius){p->state=FOLLOW;p->item=-1;g->recalls++;}}
    }else g->whistle_time=g->whistle_radius=0;
    float swarm=length(in.swarm_x,in.swarm_z);g->swarm_strength=0;
    if(swarm>.05f){float a=roundAng(atan2f(in.swarm_x,in.swarm_z)+g->yaw);g->swarm_angle=roundAng(g->swarm_angle+(1-powf(.6f,dt*30))*angDist(a,g->swarm_angle));float strength=(fminf(1,swarm)-.05f)/.95f;g->swarm_strength=strength>=.9f?1:strength/.9f*.6f;}
    for(int i=0;i<SQUAD;i++){
        Piki *p=&g->pikis[i];
        if(p->state==AIR){
            p->vel.y-=g->parms.gravity*dt;p->pos.x+=p->vel.x*dt;p->pos.y+=p->vel.y*dt;p->pos.z+=p->vel.z*dt;collide(&p->pos,4);
            if(!g->enemy.dead&&distance(p->pos,g->enemy.pos)<38&&p->pos.y<60){p->state=ATTACK;p->work_time=0;g->landings++;}
            else if(p->pos.y<=0){p->pos.y=0;p->vel=(V3){0};p->state=IDLE;g->landings++;pick_work(g,p);}
        }else if(p->state==FOLLOW){
            float side=(i%5-2)*11.f,behind=25+(i/5)*12.f;V3 target={g->captain.x+cosf(g->face)*side-sinf(g->face)*behind,0,g->captain.z-sinf(g->face)*side-cosf(g->face)*behind};
            if(g->swarm_strength>0){float forward=55+g->swarm_strength*65+(i/5)*9;target=(V3){g->captain.x+cosf(g->swarm_angle)*side+sinf(g->swarm_angle)*forward,0,g->captain.z-sinf(g->swarm_angle)*side+cosf(g->swarm_angle)*forward};}
            float dist=distance(p->pos,target);
            /* ActFormation's 7/15-unit settling and six-frame grace. */
            if(dist<=7)p->settle=0;else if(dist<15)p->settle=fminf(6,p->settle+dt*30);else p->settle=0;
            if(dist<=7||(dist<=15&&p->settle<6)){p->vel=(V3){0};p->face=roundAng(p->face+(1-powf(.7f,dt*30))*angDist(roundAng(atan2f(g->captain.x-p->pos.x,g->captain.z-p->pos.z)),p->face));}
            else walk(p,target,dist<15?g->parms.walk:g->parms.run,dt);
            if(g->swarm_strength>0&&distance(p->pos,target)<35)pick_work(g,p);
        }else if(p->state==IDLE){p->pos.y=0;pick_work(g,p);}
        else if(p->state==ATTACK){
            if(g->enemy.dead){p->state=IDLE;continue;}
            float a=i*2*PI/SQUAD;p->pos=(V3){g->enemy.pos.x+24*sinf(a),18,g->enemy.pos.z+24*cosf(a)};
            p->work_time+=dt;if(p->work_time>=.65f){p->work_time-=.65f;g->enemy.health-=10;}
        }
    }
    enemy_step(g,dt);
    for(int item=0;item<2;item++){
        CarryItem *obj=&g->items[item];if((item==1&&!g->enemy.dead)||obj->delivered)continue;
        unsigned n=assigned(g,item),slot=0;
        for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==CARRY&&g->pikis[i].item==item){float a=slot++*2*PI/fmaxf(1,n),r=item?42:30;V3 t={obj->pos.x+r*sinf(a),0,obj->pos.z+r*cosf(a)};walk(&g->pikis[i],t,g->parms.run,dt);}
        unsigned carriers=game_carriers(g,item);
        if(carriers>=(unsigned)obj->weight){
            V3 destination=item?onion_home:ship_home;float dist=distance(obj->pos,destination);
            if(dist<10){
                obj->delivered=true;if(item){g->enemy.delivered=true;g->onion_stored+=(unsigned)g->parms.carcass_yield;}else{g->delivered++;g->pokos+=(unsigned)g->parms.treasure_value;}
                for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==CARRY&&g->pikis[i].item==item){g->pikis[i].state=IDLE;g->pikis[i].item=-1;}
            }else{
                V3 next=waypoint(obj->pos,destination,item?48:35);float d=distance(obj->pos,next),speed=22+12*(carriers-obj->weight)/fmaxf(1,obj->max_carriers-obj->weight);
                if(d>1){obj->pos.x+=(next.x-obj->pos.x)/d*speed*dt;obj->pos.z+=(next.z-obj->pos.z)/d*speed*dt;}obj->lift=2;
            }
        }else obj->lift=0;
    }
    g->crate=g->items[0].pos;if(g->enemy.dead)g->enemy.pos=g->items[1].pos;
    /* New gentle ground separation prevents overlapping formation clumps. */
    for(int i=0;i<SQUAD;i++)for(int j=0;j<i;j++){
        Piki *a=&g->pikis[i],*b=&g->pikis[j];if((a->state!=FOLLOW&&a->state!=IDLE)||(b->state!=FOLLOW&&b->state!=IDLE))continue;
        float dx=a->pos.x-b->pos.x,dz=a->pos.z-b->pos.z,d=length(dx,dz);
        if(d>.001f&&d<7){float push=(7-d)*.25f;a->pos.x+=dx/d*push;a->pos.z+=dz/d*push;b->pos.x-=dx/d*push;b->pos.z-=dz/d*push;collide(&a->pos,4);collide(&b->pos,4);}
    }
    play_camera_step(g,in,dt);
}
Input game_tour(float time){(void)time;return (Input){0};}
