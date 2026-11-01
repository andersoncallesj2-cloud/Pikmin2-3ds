/* Native test-map controller. Original references (CC0 projectPiki):
 * Navi::{reviseController,makeVelocity,throwPiki,makeCStick}, NaviWhistle::update,
 * FakePiki movement, ActFormation settling and NaviFollowState normal movement.
 * World placement, local navigation/combat/storage FSMs below are adapters,
 * not the complete original Brain/ActTransport/EnemyFSM engine. */
#include "game.h"
#include "port_math.h"
#include "original_follow.h"
#include "stage.h"
#include "demo_settings.h" /* Original tuning extracted by the private builder. */
#include <math.h>
#include <string.h>
extern void original_ai_reset(void);
extern void original_ai_free(void*,unsigned,float);
#define PI 3.141592654f
static float length(float x,float z){return sqrtf(x*x+z*z);}
static float distance(V3 a,V3 b){return length(a.x-b.x,a.z-b.z);}
static float clamp(float x,float a,float b){return fmaxf(a,fminf(b,x));}
static void emit(Game *g,unsigned sound,unsigned entity,V3 pos){if(g->event_count<32){unsigned i=g->event_count++;g->events[i].sound=sound;g->events[i].entity=entity;g->events[i].pos=pos;}}
static bool on_field(const Piki *p){return p->state!=UNUSED&&p->state!=LOST&&p->state!=DYING;}
V3 game_onion_position(unsigned kind){float v[4];if(stage_landmark(kind%3+1,v))return (V3){v[0],stage_height(v[0],v[2],v[1]),v[2]};float x=-180+(kind%3)*120.f;return (V3){x,0,-300};}
V3 game_ship_position(void){float v[4];if(stage_landmark(4,v))return (V3){v[0],stage_height(v[0],v[2],v[1]),v[2]};return (V3){230,0,-320};}
float game_storage_face(unsigned kind){float v[4];return stage_landmark(kind<3?kind+1:4,v)?v[3]:0;}
V3 game_storage_point(unsigned kind,unsigned point){V3 home=kind<3?game_onion_position(kind):game_ship_position();float v[3]={0};stage_storage_point(kind<3?point%6:6+point%3,v);float a=game_storage_face(kind);return (V3){home.x+cosf(a)*v[0]+sinf(a)*v[2],home.y+v[1],home.z-sinf(a)*v[0]+cosf(a)*v[2]};}
V3 game_ship_goal(void){V3 p=game_ship_position();float a=game_storage_face(3);p.x+=135*sinf(a);p.z+=135*cosf(a);return p;}
static V3 storage_position(unsigned kind){return kind<3?game_onion_position(kind):game_ship_goal();}
bool game_in_water(V3 p){return stage_water(p.x,p.y,p.z);}
float game_day_ratio(const Game *g){return fmodf(g->time,DEMO_DAY_SECONDS)/DEMO_DAY_SECONDS;}
unsigned game_day_number(const Game *g){return 1+(unsigned)(g->time/DEMO_DAY_SECONDS);}
unsigned game_count(const Game *g,PikiState s){unsigned n=0;for(int i=0;i<SQUAD;i++)n+=g->pikis[i].state==s;return n;}
unsigned game_field_count(const Game *g){return SQUAD-game_count(g,UNUSED)-game_count(g,LOST)-game_count(g,DYING);}
unsigned game_total_count(const Game *g){unsigned n=game_field_count(g);for(unsigned k=0;k<PIKI_TYPES;k++)n+=g->onion_stored[k];return n;}
unsigned game_squad_count(const Game *g,unsigned kind){
    unsigned n=0;for(int i=0;i<SQUAD;i++){const Piki *p=&g->pikis[i];n+=(p->state==FOLLOW||p->state==HELD)&&p->owner==g->active_captain&&(kind>=PIKI_TYPES||p->kind==kind);}return n;
}
unsigned game_returnable(const Game *g,unsigned kind){unsigned n=0;for(int i=0;i<SQUAD;i++){const Piki *p=&g->pikis[i];n+=p->state==FOLLOW&&p->owner==g->active_captain&&p->kind==kind;}return n;}
static unsigned assigned(const Game *g,int item){unsigned n=0;for(int i=0;i<SQUAD;i++)n+=g->pikis[i].state==CARRY&&g->pikis[i].item==item;return n;}
unsigned game_carriers(const Game *g,int item){
    if(item<0||(unsigned)item>=g->item_count)return 0;
    unsigned n=0;const CarryItem *o=&g->items[item];
    for(int i=0;i<SQUAD;i++)n+=g->pikis[i].state==CARRY&&g->pikis[i].item==item&&distance(g->pikis[i].pos,o->pos)<o->radius+25;
    return n;
}
unsigned game_carry_strength(const Game *g,int item){unsigned n=0;const CarryItem *o=&g->items[item];for(int i=0;i<SQUAD;i++){const Piki *p=&g->pikis[i];if(p->state==CARRY&&p->item==item&&distance(p->pos,o->pos)<o->radius+16)n+=p->kind==3?10:1;}return n;}
static Piki *vacancy(Game *g){for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==UNUSED||g->pikis[i].state==LOST)return &g->pikis[i];return NULL;}
static void seed_birth(Game *g,unsigned kind,unsigned count){
    V3 home=storage_position(kind);
    for(unsigned i=0;i<count;i++){
        Piki *p=vacancy(g);if(!p){g->onion_stored[kind]+=count-i;break;}
        emit(g,0x2816,kind,home);unsigned slot=(unsigned)(p-g->pikis);float angle=slot*2.39996323f+g->time;
        *p=(Piki){.pos={home.x,home.y+110,home.z},.vel={130*sinf(angle),700,130*cosf(angle)},.state=SEED_AIR,.kind=kind,.owner=g->active_captain,.item=-1,.target=-1};
    }
}
void game_init(Game *g,Parameters p){
    memset(g,0,sizeof(*g));original_ai_reset();g->parms=p;g->captain=(V3){0,0,-220};g->other_captain=(V3){28,0,-225};
    g->bounds_x=620;g->bounds_z=650;g->dead_captain=-1;g->treasure_item=-1;g->held_piki=-1;g->onion_menu=-1;g->selected_kind=1;
    g->cursor_offset=(V3){0,0,p.cursor_radius*.5f};g->other_cursor=g->cursor_offset;
    g->cursor=(V3){g->captain.x,0,g->captain.z+g->cursor_offset.z};
    g->captain_health[0]=g->captain_health[1]=p.captain_max_health;
    for(int i=0;i<SQUAD;i++)g->pikis[i]=(Piki){.state=UNUSED,.item=-1,.target=-1};
    for(int i=0;i<30;i++)g->pikis[i]=(Piki){.pos={(i%5-2)*11.f,0,-245-(i/5)*11.f},.state=FOLLOW,.kind=i%3,.owner=0,.item=-1,.target=-1};
    for(int i=0;i<PIKI_TYPES;i++)g->onion_stored[i]=20;
    const Obstacle rocks[MAX_OBSTACLES]={{{0,0,80},35,60,55},{{-300,0,150},25,60,70},{{400,0,-80},40,35,55},{{-430,0,-260},45,45,50},{{70,0,480},50,25,45},{{-300,0,430},30,30,50}};
    memcpy(g->obstacles,rocks,sizeof(rocks));
    const Hazard hazards[MAX_HAZARDS]={{{-300,0,30},28,0},{{-450,0,320},28,0},{{-150,0,400},28,0},{{400,0,0},28,0},{{-100,0,170},29,1},{{80,0,400},29,1},{{430,0,150},29,1},{{-460,0,-100},25,2},{{450,0,260},25,2}};
    memcpy(g->hazards,hazards,sizeof(hazards));
    const V3 treasure_positions[3]={{-50,0,-60},{-420,0,180},{250,0,300}};
    for(int i=0;i<3;i++){const float *t=demo_treasure_tuning[i];g->items[g->item_count++]=(CarryItem){.pos=treasure_positions[i],.weight=(int)t[0],.max_carriers=(int)t[1],.available=true,.radius=t[3],.height=t[4],.type=ITEM_TREASURE,.model=i,.value=(unsigned)t[2],.color=1,.destination=1};}
    const unsigned enemy_kinds[MAX_ENEMIES]={0,1,2,2,0};
    const V3 enemy_positions[MAX_ENEMIES]={{-200,0,100},{320,0,20},{-400,0,450},{100,0,500},{450,0,480}};
    for(int i=0;i<MAX_ENEMIES;i++){
        unsigned kind=enemy_kinds[i];const float *t=demo_enemy_tuning[kind];unsigned item=g->item_count++;
        g->enemies[i]=(Bulborb){.pos=enemy_positions[i],.home=enemy_positions[i],.face=PI,.health=t[0],.max_health=t[0],.kind=kind,.item=item};
        g->items[item]=(CarryItem){.pos=enemy_positions[i],.weight=(int)t[5],.max_carriers=(int)t[6],.radius=t[8],.height=t[9],.type=ITEM_CARCASS,.model=(unsigned)i,.seeds=(unsigned)t[7],.matching_seeds=(unsigned)t[7],.color=1,.destination=1};
    }
    g->enemy_count=MAX_ENEMIES;
    const V3 plants[MAX_POSIES]={{-280,0,-180},{-150,0,-90},{50,0,-100},{300,0,-150},{-370,0,50},{0,0,-20},{100,0,100},{380,0,50},{-400,0,300},{-80,0,260},{150,0,460},{400,0,360},{-420,0,540},{-180,0,540},{100,0,580},{470,0,540}};
    for(int i=0;i<MAX_POSIES;i++){
        unsigned size=i/4,item=g->item_count++;const float *t=demo_pellet_tuning[size];
        g->posies[i]=(PelletPosy){.pos=plants[i],.item=item,.health=30,.scale=.55f+.15f*size,.alive=true};
        g->items[item]=(CarryItem){.pos=plants[i],.weight=(int)t[0],.max_carriers=(int)t[1],.radius=t[4],.height=t[5],.type=ITEM_PELLET,.model=size,.color=(unsigned)i%3,.destination=(unsigned)i%3,.seeds=(unsigned)t[3],.matching_seeds=(unsigned)t[2]};
    }
    g->posy_count=MAX_POSIES;g->crate=g->items[0].pos;g->population=30;play_camera_init(g);
}
static bool blocked(V3 a,V3 b,const Obstacle *box,float r){
    float lo=0,hi=1,start[]={a.x-box->pos.x,a.z-box->pos.z},delta[]={b.x-a.x,b.z-a.z},extent[]={box->half_x+r,box->half_z+r};
    for(int i=0;i<2;i++){
        if(fabsf(delta[i])<.0001f){if(fabsf(start[i])>=extent[i])return false;}
        else{float t0=(-extent[i]-start[i])/delta[i],t1=(extent[i]-start[i])/delta[i];if(t0>t1){float t=t0;t0=t1;t1=t;}lo=fmaxf(lo,t0);hi=fminf(hi,t1);if(lo>=hi)return false;}
    }
    return hi>0&&lo<1;
}
static void collide(const Game *g,V3 *p,float r){
    p->x=clamp(p->x,-g->bounds_x+r,g->bounds_x-r);p->z=clamp(p->z,-g->bounds_z+r,g->bounds_z-r);
    if(g->stage)return; /* Floor resolution is separate from horizontal collision. */
    for(unsigned i=0;i<MAX_OBSTACLES;i++){
        const Obstacle *b=&g->obstacles[i];float dx=p->x-b->pos.x,dz=p->z-b->pos.z,ex=b->half_x+r,ez=b->half_z+r;
        if(p->y<b->height&&fabsf(dx)<ex&&fabsf(dz)<ez){if(ex-fabsf(dx)<ez-fabsf(dz))p->x=b->pos.x+copysignf(ex,dx);else p->z=b->pos.z+copysignf(ez,dz);}
    }
}
static V3 waypoint(const Game *g,V3 from,V3 to,float r){
    if(g->stage)return to;
    for(unsigned obstacle=0;obstacle<MAX_OBSTACLES;obstacle++){
        const Obstacle *box=&g->obstacles[obstacle];if(!blocked(from,to,box,r))continue;
        float ex=box->half_x+r+1,ez=box->half_z+r+1,x=box->pos.x,z=box->pos.z;
        V3 nodes[]={from,to,{x-ex,0,z-ez},{x+ex,0,z-ez},{x+ex,0,z+ez},{x-ex,0,z+ez}};
        float cost[6]={0,1e9f,1e9f,1e9f,1e9f,1e9f};int prev[6]={-1,-1,-1,-1,-1,-1};bool done[6]={0};
        for(int step=0;step<6;step++){
            int u=-1;for(int i=0;i<6;i++)if(!done[i]&&(u<0||cost[i]<cost[u]))u=i;
            if(u<0||cost[u]>=1e9f)break;
            done[u]=true;
            for(int v=0;v<6;v++)if(!done[v]&&!blocked(nodes[u],nodes[v],box,r)){
                float c=cost[u]+distance(nodes[u],nodes[v]);if(c<cost[v]){cost[v]=c;prev[v]=u;}
            }
        }
        if(prev[1]<0)return to;
        int at=1;while(prev[at]>0)at=prev[at];return nodes[at];
    }
    return to;
}
static void walk(Game *g,Piki *p,V3 goal,float speed,float dt){
    V3 t=waypoint(g,p->pos,goal,4);float dx=t.x-p->pos.x,dz=t.z-p->pos.z,d=length(dx,dz);speed=fminf(speed*(p->kind==4?DEMO_WHITE_SPEED:p->kind==3?DEMO_PURPLE_SPEED:1),d/dt);
    p->vel.x=d>.2f?dx/d*speed:0;p->vel.z=d>.2f?dz/d*speed:0;p->pos.x+=p->vel.x*dt;p->pos.z+=p->vel.z*dt;
    if(d>1)p->face=roundAng(p->face+fminf(1,dt*8)*angDist(roundAng(atan2f(dx,dz)),p->face));
    collide(g,&p->pos,4);
}
static void die(Game *g,Piki *p){if(on_field(p)){emit(g,0x280d,(unsigned)(p-g->pikis),p->pos);p->state=DYING;p->work_time=0;p->item=-1;g->deaths++;}}
static void work(Game *g,Piki *p){
    for(unsigned e=0;e<g->enemy_count;e++)if(!g->enemies[e].dead&&distance(p->pos,g->enemies[e].pos)<demo_enemy_tuning[g->enemies[e].kind][8]+20){p->state=ATTACK;p->target=(int)e;p->work_time=0;return;}
    for(unsigned i=0;i<g->posy_count;i++)if(g->posies[i].alive&&distance(p->pos,g->posies[i].pos)<30){p->state=ATTACK;p->target=-(int)i-1;p->work_time=0;return;}
    int best=-1;float nearest=1e9f;
    for(unsigned i=0;i<g->item_count;i++){
        CarryItem *o=&g->items[i];float d=distance(p->pos,o->pos);
        if(o->available&&!o->delivered&&d<o->radius+30&&assigned(g,i)<(unsigned)o->max_carriers&&d<nearest){best=(int)i;nearest=d;}
    }
    if(best>=0){p->state=CARRY;p->item=best;}
}
static bool pluck(Game *g){
    for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==SPROUT&&distance(g->pikis[i].pos,g->captain)<25){
        g->pikis[i].state=PLUCKING;g->pikis[i].work_time=0;g->pikis[i].owner=g->active_captain;g->pikis[i].pos.y=stage_height(g->pikis[i].pos.x,g->pikis[i].pos.z,0);g->plucks++;emit(g,0x802,i,g->pikis[i].pos);g->plucking=true;g->pluck_timer=.35f;return true;
    }
    return false;
}
static void onion_menu(Game *g,Input *in){
    unsigned color=(unsigned)g->onion_menu;if(color>=PIKI_TYPES)return;
    unsigned pending=0;for(unsigned k=0;k<PIKI_TYPES;k++)pending+=g->withdraw[k];
    int capacity=(int)SQUAD-(int)game_field_count(g)-(int)pending,take=(int)g->onion_stored[color]-(int)g->withdraw[color];if(capacity<0)capacity=0;if(take>capacity)take=capacity;
    g->onion_adjust=(int)clamp(g->onion_adjust+in->menu_delta,-(float)game_returnable(g,color),(float)take);
    if(in->menu_accept){
        if(g->onion_adjust>0)g->withdraw[color]+=(unsigned)g->onion_adjust;
        else{int remaining=-g->onion_adjust;for(int i=0;i<SQUAD&&remaining;i++){Piki *p=&g->pikis[i];if(p->state==FOLLOW&&p->owner==g->active_captain&&p->kind==color){p->state=ENTER_ONION;p->item=(int)color;p->storage_leg=(unsigned)i%3;p->storage_phase=0;p->travel=0;p->storage_scale=1;remaining--;}}}
        g->onion_menu=-1;g->onion_adjust=0;
    }else if(in->menu_cancel){g->onion_menu=-1;g->onion_adjust=0;}
    in->x=in->z=0;in->grab_pressed=in->throw_pressed=in->whistle=false;
}
static void throw_piki(Game *g){
    if(g->held_piki<0)return;
    Piki *p=&g->pikis[g->held_piki];p->state=AIR;p->rejoin=false;
    p->pos=(V3){g->captain.x+8*sinf(g->face),g->captain.y+10,g->captain.z+8*cosf(g->face)};emit(g,0x806,0,p->pos);emit(g,0x2803,(unsigned)(p-g->pikis),p->pos);
    float height=p->kind==2?DEMO_YELLOW_THROW:p->kind==3?g->parms.throw_height*.65f:g->parms.throw_height,half=.5f*g->parms.landing_time;
    float vy=half*(.5f*g->parms.gravity)+height/half,time=2*vy/g->parms.gravity;
    p->vel=(V3){(g->cursor.x-p->pos.x)/time+g->velocity.x,vy,(g->cursor.z-p->pos.z)/time+g->velocity.z};g->throws++;g->held_piki=-1;g->captain_action=5;g->action_timer=.5f;
}
static void elemental_hazards(Game *g,Piki *p,float dt){
    if(!on_field(p)||p->state==SPROUT||p->state==SEED_AIR||p->state==HELD||p->state==SHOCKED||p->state==ENTER_ONION||p->state==EXIT_ONION||p->pos.y>stage_height(p->pos.x,p->pos.z,p->pos.y)+20)return;
    for(unsigned i=0;i<MAX_HAZARDS;i++)if(distance(p->pos,g->hazards[i].pos)<g->hazards[i].radius){
        if(g->hazards[i].kind==1&&p->kind!=2){p->state=SHOCKED;p->work_time=0;p->item=-1;emit(g,0x284c,i,p->pos);return;}
        if(g->hazards[i].kind==2&&p->kind!=4&&p->state!=POISONED){p->state=POISONED;p->work_time=0;p->item=-1;emit(g,0x280c,i,p->pos);}
        if(g->hazards[i].kind==0&&p->kind!=1&&p->state!=BURNING){emit(g,0x280c,i,p->pos);p->state=BURNING;p->work_time=0;p->item=-1;p->target=(int)i;}
    }
    if(game_in_water(p->pos)&&p->kind!=0&&p->state!=DROWNING){p->state=DROWNING;p->work_time=0;p->item=-1;p->called=false;}
    if(p->state==POISONED){p->work_time+=dt;if(p->work_time>5)die(g,p);}
    else if(p->state==BURNING){
        Hazard *h=&g->hazards[p->target];float a=atan2f(p->pos.x-h->pos.x,p->pos.z-h->pos.z);
        V3 escape={p->pos.x+50*sinf(a),0,p->pos.z+50*cosf(a)};walk(g,p,escape,g->parms.run*1.3f,dt);p->work_time+=dt;if(p->work_time>DEMO_FIRE_TIME)die(g,p);
    }else if(p->state==DROWNING){
        p->work_time+=dt;
        if(p->called)walk(g,p,g->captain,g->parms.walk+(g->parms.run-g->parms.walk)*.5f,dt);
        if(!game_in_water(p->pos)){p->state=FOLLOW;p->owner=g->active_captain;p->pos.y=0;}
        else if(p->work_time>10)die(g,p);
    }else if(p->state==FOLLOW||p->state==IDLE||p->state==CARRY||p->state==ENTER_ONION)p->pos.y=stage_height(p->pos.x,p->pos.z,p->pos.y)+(game_in_water(p->pos)?-4:0);
}
static void enemy_step(Game *g,unsigned id,float dt){
    Bulborb *e=&g->enemies[id];const float *t=demo_enemy_tuning[e->kind];if(e->dead)return;
    if(e->health<=0){emit(g,e->kind==2?0x581c:0x5818,id,e->pos);e->dead=true;e->state=3;g->items[e->item].pos=e->pos;g->items[e->item].available=true;return;}
    e->cooldown=fmaxf(0,e->cooldown-dt);V3 target=g->captain;float nearest=distance(e->pos,target);
    for(int i=0;i<SQUAD;i++)if(on_field(&g->pikis[i])&&g->pikis[i].state!=ATTACK&&g->pikis[i].state!=SPROUT&&g->pikis[i].pos.y<25){float d=distance(e->pos,g->pikis[i].pos);if(d<nearest){nearest=d;target=g->pikis[i].pos;}}
    if(e->state==0&&nearest>t[2]&&e->health>=e->max_health)return;
    if(nearest>240&&distance(e->pos,e->home)<15){e->dormant+=dt;if(e->dormant>5){e->state=0;e->cooldown=0;return;}}else e->dormant=0;
    if(e->stun>0){e->stun-=dt;return;}
    e->state=1;if(nearest>240||distance(e->home,e->pos)>220)target=e->home;
    float angle=roundAng(atan2f(target.x-e->pos.x,target.z-e->pos.z));e->face=roundAng(e->face+fminf(1,dt*2)*angDist(angle,e->face));
    if(nearest>t[3]*.75f){
        V3 next=waypoint(g,e->pos,target,t[8]*.5f);float d=distance(e->pos,next);
        if(d>1){e->pos.x+=(next.x-e->pos.x)/d*t[1]*dt;e->pos.z+=(next.z-e->pos.z)/d*t[1]*dt;collide(g,&e->pos,t[8]*.5f);}
    }else if(e->cooldown<=0){
        emit(g,e->kind==2?0x581e:0x5815,id,e->pos);e->state=2;e->cooldown=e->kind==2?1.3f:1.8f;int eaten=0;
        for(int i=0;i<SQUAD&&eaten<(e->kind==2?1:2);i++){
            Piki *p=&g->pikis[i];float a=atan2f(p->pos.x-e->pos.x,p->pos.z-e->pos.z);
            if(on_field(p)&&p->state!=HELD&&p->state!=ATTACK&&p->state!=SPROUT&&p->pos.y<e->pos.y+25&&distance(p->pos,e->pos)<t[3]&&cosf(a-e->face)>.2f){if(p->kind==4)e->health-=150;die(g,p);emit(g,e->kind==2?0x581f:0x5819,id,e->pos);eaten++;}
        }
        if(distance(g->captain,e->pos)<t[3])g->captain_health[g->active_captain]=fmaxf(0,g->captain_health[g->active_captain]-t[4]);
    }
}
static void follow_captain(Game *g,float dt){
    if(!g->other_following||g->captain_health[g->active_captain^1]<=0)return;
    if(g->plucking){g->other_pluck_timer-=dt;if(g->other_pluck_timer<=0){for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==SPROUT&&distance(g->pikis[i].pos,g->other_captain)<35){Piki *p=&g->pikis[i];p->state=PLUCKING;p->work_time=0;p->owner=g->active_captain;g->plucks++;g->other_pluck_timer=.4f;emit(g,0x802,i,p->pos);break;}}}
    V3 target=g->captain;
    if(g->held_piki>=0&&length(g->velocity.x,g->velocity.z)<20){target.x+=30*sinf(g->face+1.4137167f);target.z+=30*cosf(g->face+1.4137167f);}
    float velocity[3],leader[]={target.x,target.y,target.z},motion[]={g->velocity.x,0,g->velocity.z},follower[]={g->other_captain.x,g->other_captain.y,g->other_captain.z};
    if(!original_follow_velocity(leader,motion,follower,g->parms.move,velocity)){g->other_following=false;return;}
    if(length(velocity[0],velocity[2])>1){V3 next=waypoint(g,g->other_captain,target,8.5f);float d=distance(next,g->other_captain),speed=length(velocity[0],velocity[2]);if(d>1){velocity[0]=(next.x-g->other_captain.x)/d*speed;velocity[2]=(next.z-g->other_captain.z)/d*speed;}}
    g->other_velocity.x+=(velocity[0]-g->other_velocity.x)*dt/.1f;g->other_velocity.z+=(velocity[2]-g->other_velocity.z)*dt/.1f;
    g->other_captain.x+=g->other_velocity.x*dt;g->other_captain.z+=g->other_velocity.z*dt;collide(g,&g->other_captain,8.5f);
    if(length(velocity[0],velocity[2])>1)g->other_face=roundAng(atan2f(velocity[0],velocity[2]));
}
/* Adapted from Navi::releasePikis: colour means, sqrt(N)*6.25 radii,
 * four separation passes and clearance from both captains. Native walk FSM
 * replaces ActFree/InteractKaisan; positions remain collision checked. */
static void dismiss_colours(Game *g){
    emit(g,g->active_captain?0x8a1:0x808,0,g->captain);
    V3 centers[PIKI_TYPES]={{0}};unsigned counts[PIKI_TYPES]={0},rank[PIKI_TYPES]={0};float radius[PIKI_TYPES]={0};
    for(int i=0;i<SQUAD;i++){Piki *p=&g->pikis[i];if(p->state==FOLLOW&&p->owner==g->active_captain){centers[p->kind].x+=p->pos.x;centers[p->kind].z+=p->pos.z;counts[p->kind]++;}}
    for(unsigned c=0;c<PIKI_TYPES;c++)if(counts[c]){centers[c].x/=counts[c];centers[c].z/=counts[c];radius[c]=sqrtf(counts[c])*6.25f;}
    for(unsigned pass=0;pass<4;pass++)for(unsigned c=0;c<PIKI_TYPES;c++)if(counts[c]){
        V3 captains[]={g->captain,g->other_captain};
        for(unsigned n=0;n<2;n++){float dx=centers[c].x-captains[n].x,dz=centers[c].z-captains[n].z,d=length(dx,dz),min=radius[c]+45;if(d<min){float a=g->face+c*2*PI/PIKI_TYPES;if(d<.01f){dx=sinf(a);dz=cosf(a);d=1;}centers[c].x+=dx/d*(min-d);centers[c].z+=dz/d*(min-d);}}
        for(unsigned j=c+1;j<PIKI_TYPES;j++)if(counts[j]){float dx=centers[c].x-centers[j].x,dz=centers[c].z-centers[j].z,d=length(dx,dz),min=radius[c]+radius[j]+20;if(d<min){if(d<.01f){dx=1;dz=0;d=1;}float push=(min-d);centers[c].x+=dx/d*push;centers[c].z+=dz/d*push;centers[j].x-=dx/d*push;centers[j].z-=dz/d*push;}}
    }
    for(int i=0;i<SQUAD;i++){Piki *p=&g->pikis[i];if(p->state==FOLLOW&&p->owner==g->active_captain){unsigned n=rank[p->kind]++;float a=n*2.39996323f,r=6.25f*sqrtf(n);p->disband_target=(V3){centers[p->kind].x+r*sinf(a),0,centers[p->kind].z+r*cosf(a)};collide(g,&p->disband_target,4);p->state=DISBANDING;p->item=-1;p->work_time=0;}}
    g->other_following=false;
}
/* Adapted ActEnter/ActExit/ActClimb geometry, speeds and shrink formulas.
 * Original collision attachment coordinates come from the disc. Native state
 * dispatch replaces Brain/startStick; it is not the complete original actions. */
static void storage_motion(Game *g,Piki *p,float dt){
    bool entering=p->state==ENTER_ONION;
    if(p->kind<3){
        V3 top=game_storage_point(p->kind,p->storage_leg*2),foot=game_storage_point(p->kind,p->storage_leg*2+1);
        float dx=foot.x-top.x,dy=foot.y-top.y,dz=foot.z-top.z,len=sqrtf(dx*dx+dy*dy+dz*dz);
        if(entering&&p->storage_phase==0){walk(g,p,foot,g->parms.run,dt);if(distance(p->pos,foot)>10)return;p->storage_phase=1;p->travel=0;emit(g,0x2815,(unsigned)(p-g->pikis),p->pos);}
        p->travel=fminf(1,p->travel+(entering?50:100)*dt/fmaxf(1,len));float t=entering?1-p->travel:p->travel;
        p->pos=(V3){top.x+dx*t,top.y+dy*t,top.z+dz*t};p->face=atan2f(entering?-dx:dx,entering?-dz:dz);p->vel=(V3){0};p->storage_scale=fminf(1,t/.25f);
        if(p->travel>=1){if(entering){g->onion_stored[p->kind]++;p->state=UNUSED;emit(g,0x281d,(unsigned)(p-g->pikis),p->pos);}else{p->state=FOLLOW;p->storage_scale=1;p->owner=g->active_captain;}}
    }else{
        V3 end=game_storage_point(p->kind,0),home=game_ship_position();
        if(p->storage_phase==0){float dx=end.x-home.x,dz=end.z-home.z,d=length(dx,dz);V3 start={home.x+dx/fmaxf(1,d)*90,home.y,home.z+dz/fmaxf(1,d)*90};walk(g,p,start,g->parms.run,dt);if(distance(p->pos,start)>20)return;p->storage_phase=1;p->work_time=1.3f;}
        if(p->storage_phase==1){p->work_time-=dt;p->vel=(V3){0};if(p->work_time>0)return;p->storage_phase=2;p->travel=0;p->disband_target=p->pos;emit(g,0x281f,(unsigned)(p-g->pikis),p->pos);}
        V3 start=p->disband_target;float horizontal=distance(start,end);p->travel=fminf(1,p->travel+dt*110/fmaxf(1,horizontal));float t=p->travel;
        p->pos=(V3){start.x+(end.x-start.x)*t,start.y+(end.y-3-start.y)*(2*t-t*t),start.z+(end.z-start.z)*t};p->storage_scale=fmaxf(.05f,sqrtf(1-t));
        if(t>=1){g->onion_stored[p->kind]++;p->state=UNUSED;emit(g,0x281d,(unsigned)(p-g->pikis),p->pos);}
    }
}
static void withdraw_step(Game *g,float dt){
    for(unsigned kind=0;kind<PIKI_TYPES;kind++)if(g->withdraw[kind]){
        if(g->release_clock[kind]>0){g->release_clock[kind]-=dt;continue;}
        Piki *p=vacancy(g);if(!p||!g->onion_stored[kind]){g->withdraw[kind]=0;continue;}
        unsigned index=(unsigned)(p-g->pikis);*p=(Piki){.kind=kind,.owner=g->active_captain,.item=-1,.target=-1,.storage_leg=index%3,.storage_scale=0,.face=game_storage_face(kind)};
        if(kind<3){p->state=EXIT_ONION;p->pos=game_storage_point(kind,p->storage_leg*2);}
        else{V3 home=game_ship_position();p->pos=game_storage_point(kind,1);V3 v={p->pos.x-home.x,p->pos.y-home.y,p->pos.z-home.z};float d=sqrtf(v.x*v.x+v.y*v.y+v.z*v.z),factor=100+(index%31);p->vel=(V3){v.x/fmaxf(1,d)*factor,v.y/fmaxf(1,d)*factor,v.z/fmaxf(1,d)*factor};p->state=AIR;p->rejoin=true;}
        emit(g,0x281e,index,p->pos);g->onion_stored[kind]--;g->withdraw[kind]--;g->release_clock[kind]=.01f+(index%6)*.001f;
    }
}
void game_step(Game *g,Input in,float dt){
    if(in.reset){Parameters p=g->parms;game_init(g,p);return;}if(!(dt>0&&dt<=.05f))return;
    g->event_count=0;
    g->switch_timer=fmaxf(0,g->switch_timer-dt);
    if(g->game_over)return;
    if(g->captain_health[g->active_captain]<=0){
        if(g->dead_captain<0){g->dead_captain=(int)g->active_captain;g->death_timer=2;g->velocity=(V3){0};g->other_following=false;g->plucking=false;g->onion_menu=-1;if(g->held_piki>=0){g->pikis[g->held_piki].state=IDLE;g->held_piki=-1;}emit(g,0x811,0,g->captain);}
        g->death_timer-=dt;in=(Input){0};
        if(g->death_timer<=0){if(g->captain_health[g->active_captain^1]>0){in.switch_pressed=true;g->dead_captain=-1;}else {g->game_over=true;return;}}
    }
    g->time+=dt;g->steps++;g->pluck_timer=fmaxf(0,g->pluck_timer-dt);g->action_timer=fmaxf(0,g->action_timer-dt);
    if(g->onion_menu>=0){onion_menu(g,&in);return;}
    withdraw_step(g,dt);
    if(in.switch_pressed&&g->held_piki<0&&g->captain_health[g->active_captain^1]>0){
        V3 p=g->captain;g->captain=g->other_captain;g->other_captain=p;p=g->cursor_offset;g->cursor_offset=g->other_cursor;g->other_cursor=p;
        float f=g->face;g->face=g->other_face;g->other_face=f;g->velocity=(V3){0};g->other_velocity=(V3){0};g->active_captain^=1;g->switch_timer=.65f;g->captain_action=12;g->action_timer=.65f;emit(g,g->active_captain?0x188e:0x188d,0,g->captain);
        if(g->other_following)for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==FOLLOW)g->pikis[i].owner=g->active_captain;
        g->camera.follow_time=g->parms.camera[g->active_captain].follow;
    }
    if(g->captain_health[g->active_captain]<=0)in.x=in.z=0;
    float mag=length(in.x,in.z),x=0,z=0;
    if(mag>0){
        float a=roundAng(atan2f(in.x,in.z)),bin=g->parms.shake*PI/180;a=bin*(int)((bin*.5f+a)/bin);
        float boost=a-(int)(a/(PI/4))*(PI/4);mag*=(sinf(boost)+sinf(PI/4-boost))/sinf(PI/4);
        if(mag>=g->parms.stick_clamp)mag=1;
        if(mag<g->parms.neutral)mag=0;
        x=sinf(a+g->yaw)*mag;z=cosf(a+g->yaw)*mag;
    }
    g->still_time=mag>g->parms.neutral?0:g->still_time+dt;float vx=x*g->parms.move,vz=z*g->parms.move;
    if((mag>g->parms.neutral||g->still_time>=g->parms.cursor_look_time)&&mag<=g->parms.cursor_threshold){vx=vz=0;float a=roundAng(atan2f(g->cursor_offset.x,g->cursor_offset.z));g->face=roundAng(g->face+(1-powf(.8f,dt*30))*angDist(a,g->face));}
    else if(mag>g->parms.neutral)g->face=roundAng(g->face+8*dt*angDist(roundAng(atan2f(x,z)),g->face));
    g->velocity.x+=(vx-g->velocity.x)*dt/.1f;g->velocity.z+=(vz-g->velocity.z)*dt/.1f;g->captain.x+=g->velocity.x*dt;g->captain.z+=g->velocity.z*dt;collide(g,&g->captain,8.5f);
    float d=length(x,z);
    if(d>0){float sx=x/d*g->parms.cursor_speed,sz=z/d*g->parms.cursor_speed;V3 next={g->cursor_offset.x+sx*dt,0,g->cursor_offset.z+sz*dt};float r=length(next.x,next.z);if(r>=g->parms.cursor_radius){float nx=next.x/r,nz=next.z/r,dot=nx*sx+nz*sz;next=(V3){g->cursor_offset.x+(sx-nx*dot)*dt,0,g->cursor_offset.z+(sz-nz*dot)*dt};}g->cursor_offset=next;}
    g->cursor=(V3){g->captain.x+g->cursor_offset.x,0,g->captain.z+g->cursor_offset.z};g->cursor.y=stage_height(g->cursor.x,g->cursor.z,g->captain.y);g->aim_distance=length(g->cursor_offset.x,g->cursor_offset.z);
    if(in.dismiss)dismiss_colours(g);
    if(!in.grab_held)g->plucking=false;
    if(g->plucking&&in.grab_held&&g->pluck_timer<=0)pluck(g);
    if(in.grab_pressed&&g->held_piki<0&&g->onion_menu<0){
        if(pluck(g))in.grab_pressed=false;
        else for(unsigned kind=0;kind<PIKI_TYPES;kind++)if(distance(g->captain,storage_position(kind))<45){g->onion_menu=(int)kind;g->onion_adjust=0;in.grab_pressed=false;break;}
        if(in.grab_pressed){
            for(int pass=0;pass<2&&g->held_piki<0;pass++)for(int i=0;i<SQUAD;i++){
                Piki *p=&g->pikis[i];if(p->state==FOLLOW&&p->owner==g->active_captain&&(pass||p->kind==g->selected_kind)&&distance(p->pos,g->captain)<85){g->held_piki=i;p->state=HELD;emit(g,0x2802,i,p->pos);g->selected_kind=p->kind;break;}
            }
            if(g->held_piki<0){
                g->captain_action=3;g->action_timer=.4f;
                for(unsigned e=0;e<g->enemy_count;e++)if(!g->enemies[e].dead&&distance(g->enemies[e].pos,g->captain)<demo_enemy_tuning[g->enemies[e].kind][8]+g->parms.punch_range)g->enemies[e].health-=g->parms.punch_damage;
                for(unsigned p=0;p<g->posy_count;p++)if(g->posies[p].alive&&distance(g->posies[p].pos,g->captain)<30)g->posies[p].health-=g->parms.punch_damage;
            }
        }
    }
    if(g->held_piki>=0&&in.cycle_pressed){
        unsigned start=g->pikis[g->held_piki].kind;
        for(unsigned pass=1;pass<=PIKI_TYPES;pass++){
            unsigned kind=(start+pass)%PIKI_TYPES;int found=-1;for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==FOLLOW&&g->pikis[i].owner==g->active_captain&&g->pikis[i].kind==kind&&distance(g->pikis[i].pos,g->captain)<85){found=i;break;}
            if(found>=0){g->pikis[g->held_piki].state=FOLLOW;g->held_piki=found;g->pikis[found].state=HELD;g->selected_kind=kind;break;}
        }
    }
    if(in.throw_pressed||(g->held_piki>=0&&!in.grab_held))throw_piki(g);
    if(g->held_piki>=0)g->pikis[g->held_piki].pos=(V3){g->captain.x+8*sinf(g->face),g->captain.y+10,g->captain.z+8*cosf(g->face)};
    g->calling=in.whistle&&g->held_piki<0&&g->onion_menu<0;
    if(g->calling){
        g->whistle_time=fminf(g->parms.call_time,g->whistle_time+dt);g->whistle_radius=g->parms.call_min+(g->parms.call_max-g->parms.call_min)*g->whistle_time/g->parms.call_time;
        for(int i=0;i<SQUAD;i++){
            Piki *p=&g->pikis[i];if(!on_field(p)||p->state==AIR||p->state==HELD||p->state==SPROUT||p->state==SEED_AIR||p->state==SHOCKED||p->state==ENTER_ONION||p->state==EXIT_ONION)continue;
            if(distance(p->pos,g->cursor)<=g->whistle_radius){
                if(p->state==DROWNING){p->called=true;p->work_time=fmaxf(0,p->work_time-dt);}
                else if(p->state!=FOLLOW||p->owner!=g->active_captain){p->state=FOLLOW;p->owner=g->active_captain;p->item=-1;g->recalls++;}
            }
        }
        if(g->captain_health[g->active_captain^1]>0&&distance(g->other_captain,g->cursor)<=g->whistle_radius){
            g->other_following=true;for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==FOLLOW)g->pikis[i].owner=g->active_captain;
        }
    }else g->whistle_time=g->whistle_radius=0;
    float swarm=length(in.swarm_x,in.swarm_z);g->swarm_strength=0;
    if(swarm>.05f&&g->onion_menu<0){float a=roundAng(atan2f(in.swarm_x,in.swarm_z)+g->yaw);g->swarm_angle=roundAng(g->swarm_angle+(1-powf(.6f,dt*30))*angDist(a,g->swarm_angle));float strength=(fminf(1,swarm)-.05f)/.95f;g->swarm_strength=strength>=.9f?1:strength/.9f*.6f;}
    unsigned ranks[2]={0,0};
    for(int i=0;i<SQUAD;i++){
        Piki *p=&g->pikis[i];if(p->state==DYING){p->work_time+=dt;if(p->work_time>1.5f)p->state=LOST;continue;}if(!on_field(p))continue;
        if(p->state==AIR||p->state==SEED_AIR){
            bool seed=p->state==SEED_AIR;p->vel.y-=g->parms.gravity*dt;p->pos.x+=p->vel.x*dt;p->pos.y+=p->vel.y*dt;p->pos.z+=p->vel.z*dt;collide(g,&p->pos,4);
            if(!seed&&!p->rejoin)for(unsigned e=0;e<g->enemy_count;e++)if(!g->enemies[e].dead&&distance(p->pos,g->enemies[e].pos)<demo_enemy_tuning[g->enemies[e].kind][8]&&p->pos.y<g->enemies[e].pos.y+demo_enemy_tuning[g->enemies[e].kind][9]+40){
                Bulborb *enemy=&g->enemies[e];if(enemy->kind==2&&p->vel.y>=0)continue;float center=distance(p->pos,enemy->pos);
                if((enemy->kind==2&&center<demo_enemy_tuning[2][8]*.55f&&p->vel.y<0)||p->kind==3){enemy->health-=enemy->kind==2?enemy->max_health:50;enemy->stun=p->kind==3?2:0;emit(g,0x2808,i,p->pos);}
                p->state=ATTACK;p->target=(int)e;p->work_time=0;g->landings++;break;}
            if((p->state==AIR||p->state==SEED_AIR)&&p->pos.y<=stage_height(p->pos.x,p->pos.z,p->pos.y)){p->pos.y=stage_height(p->pos.x,p->pos.z,p->pos.y);emit(g,seed?0x2817:0x2804,i,p->pos);p->vel=(V3){0};if(seed)p->state=SPROUT;else{if(p->kind==3)for(unsigned e=0;e<g->enemy_count;e++)if(!g->enemies[e].dead&&distance(p->pos,g->enemies[e].pos)<45){g->enemies[e].health-=50;g->enemies[e].stun=2;emit(g,0x5813,e,g->enemies[e].pos);}p->state=p->rejoin?FOLLOW:IDLE;g->landings++;if(!p->rejoin)work(g,p);}}
        }else if(p->state==FOLLOW){
            bool active=p->owner==g->active_captain;V3 leader=active?g->captain:g->other_captain;float face=active?g->face:g->other_face;
            unsigned rank=ranks[p->owner%2]++;float side=((int)(rank%7)-3)*10.f,behind=24+(rank/7)*11.f;
            V3 target={leader.x+cosf(face)*side-sinf(face)*behind,0,leader.z-sinf(face)*side-cosf(face)*behind};
            if(active&&g->swarm_strength>0){float forward=55+g->swarm_strength*65+(rank/7)*9;target=(V3){leader.x+cosf(g->swarm_angle)*side+sinf(g->swarm_angle)*forward,0,leader.z-sinf(g->swarm_angle)*side+cosf(g->swarm_angle)*forward};}
            float dist=distance(p->pos,target);if(dist<=7)p->settle=0;else if(dist<15)p->settle=fminf(6,p->settle+dt*30);else p->settle=0;
            if(dist<=7||(dist<=15&&p->settle<6))p->vel=(V3){0};else walk(g,p,target,dist<15?g->parms.walk:g->parms.run,dt);
            if(active&&g->swarm_strength>0&&distance(p->pos,target)<35)work(g,p);
        }else if(p->state==DISBANDING){walk(g,p,p->disband_target,g->parms.run,dt);p->work_time+=dt;if(distance(p->pos,p->disband_target)<5||p->work_time>5){p->state=IDLE;p->vel=(V3){0};}}
        else if(p->state==IDLE){original_ai_free(g,(unsigned)i,dt);}
        else if(p->state==CELEBRATE||p->state==PLUCKING){p->work_time+=dt;if(p->work_time>.8f)p->state=p->state==PLUCKING?FOLLOW:IDLE;}
        else if(p->state==SHOCKED){p->work_time+=dt;if(p->work_time>.6f)die(g,p);}
        else if(p->state==ENTER_ONION||p->state==EXIT_ONION)storage_motion(g,p,dt);
        else if(p->state==ATTACK){
            bool dead;V3 target;float radius,height;
            if(p->target>=0){Bulborb *e=&g->enemies[p->target];dead=e->dead;target=e->pos;radius=demo_enemy_tuning[e->kind][8]*.6f;height=demo_enemy_tuning[e->kind][9]*.5f;}
            else{PelletPosy *plant=&g->posies[-p->target-1];dead=!plant->alive;target=plant->pos;radius=12;height=0;}
            if(dead){p->state=IDLE;p->pos.y=stage_height(p->pos.x,p->pos.z,p->pos.y);work(g,p);}
            else{float a=i*2*PI/30;p->pos=(V3){target.x+radius*sinf(a),target.y+height,target.z+radius*cosf(a)};p->work_time+=dt;if(p->work_time>=.65f){p->work_time-=.65f;float damage=demo_piki_damage[p->kind];emit(g,0x2808,i,p->pos);if(p->target>=0){g->enemies[p->target].health-=damage;emit(g,g->enemies[p->target].kind==2?0x581b:0x5812,p->target,p->pos);}else g->posies[-p->target-1].health-=damage;}}
        }
        elemental_hazards(g,p,dt);
    }
    /* Blue Pikmin's rescue role: throw a nearby drowning Pikmin to shore. */
    for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==DROWNING){Piki *p=&g->pikis[i];for(int j=0;j<SQUAD;j++)if(g->pikis[j].kind==0&&(g->pikis[j].state==FOLLOW||g->pikis[j].state==IDLE)&&distance(p->pos,g->pikis[j].pos)<35){
        V3 shore=p->pos;float edges[]={p->pos.x-100,380-p->pos.x,p->pos.z-175,425-p->pos.z};int edge=0;for(int e=1;e<4;e++)if(edges[e]<edges[edge])edge=e;
        if(edge==0)shore.x=85;else if(edge==1)shore.x=395;else if(edge==2)shore.z=160;else shore.z=440;
        p->state=AIR;p->rejoin=true;p->vel=(V3){(shore.x-p->pos.x)/.55f,160,(shore.z-p->pos.z)/.55f};break;
    }}
    for(unsigned e=0;e<g->enemy_count;e++)enemy_step(g,e,dt);
    for(unsigned p=0;p<g->posy_count;p++){
        PelletPosy *plant=&g->posies[p];CarryItem *item=&g->items[plant->item];
        if(plant->alive){item->color=((unsigned)(g->time/3)+p)%3;if(plant->health<=0){plant->alive=false;item->available=true;item->pos=plant->pos;}}
    }
    for(unsigned item=0;item<g->item_count;item++){
        CarryItem *o=&g->items[item];if(!o->available||o->delivered)continue;
        if(o->collecting){o->suction+=dt;o->lift+=75*dt;if(o->suction>=1.5f){o->delivered=true;if(o->type==ITEM_TREASURE){g->delivered++;g->pokos+=o->value;g->treasure_item=(int)item;g->treasure_timer=0;}else seed_birth(g,o->destination,o->type==ITEM_PELLET&&o->destination==o->color?o->matching_seeds:o->seeds);}continue;}
        unsigned n=assigned(g,item),slot=0,colors[PIKI_TYPES]={0};
        for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==CARRY&&g->pikis[i].item==(int)item){float a=slot++*2*PI/fmaxf(1,n),r=o->radius+6;V3 t={o->pos.x+r*sinf(a),0,o->pos.z+r*cosf(a)};walk(g,&g->pikis[i],t,g->parms.run,dt);if(distance(g->pikis[i].pos,o->pos)<o->radius+16)colors[g->pikis[i].kind]++;}
        unsigned carriers=game_carry_strength(g,item),goal=o->destination;
        for(unsigned c=0;c<3;c++)if(colors[c]>colors[goal])goal=c;
        o->destination=goal;
        if(carriers>=(unsigned)o->weight){
            V3 destination=o->type==ITEM_TREASURE?game_ship_goal():game_onion_position(goal);float dist=distance(o->pos,destination);
            if(dist<12){
                o->collecting=true;o->suction=0;emit(g,0x3833,item,o->pos);
                for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==CARRY&&g->pikis[i].item==(int)item){g->pikis[i].state=CELEBRATE;g->pikis[i].work_time=0;g->pikis[i].item=-1;}

            }else{
                V3 next=waypoint(g,o->pos,destination,o->radius+5);float d=distance(o->pos,next),speed=22+12*fminf(1,(carriers-o->weight)/fmaxf(1,o->max_carriers-o->weight));
                if(d>1){o->pos.x+=(next.x-o->pos.x)/d*speed*dt;o->pos.z+=(next.z-o->pos.z)/d*speed*dt;}o->lift=2;
            }
        }else o->lift=0;
        if(g->stage&&!o->collecting)o->pos.y=stage_height(o->pos.x,o->pos.z,o->pos.y);
    }
    for(unsigned e=0;e<g->enemy_count;e++)if(g->enemies[e].dead){g->enemies[e].pos=g->items[g->enemies[e].item].pos;g->enemies[e].delivered=g->items[g->enemies[e].item].delivered;}
    /* Local separation is new collision glue, not original CPlate code. */
    for(int i=0;i<SQUAD;i++)for(int j=0;j<i;j++){
        Piki *a=&g->pikis[i],*b=&g->pikis[j];if((a->state!=FOLLOW&&a->state!=IDLE)||(b->state!=FOLLOW&&b->state!=IDLE))continue;
        float dx=a->pos.x-b->pos.x,dz=a->pos.z-b->pos.z,d=length(dx,dz);if(d>.001f&&d<7){float push=(7-d)*.25f;a->pos.x+=dx/d*push;a->pos.z+=dz/d*push;b->pos.x-=dx/d*push;b->pos.z-=dz/d*push;collide(g,&a->pos,4);collide(g,&b->pos,4);}
    }
    if(g->held_piki<0&&!game_returnable(g,g->selected_kind))for(unsigned k=0;k<PIKI_TYPES;k++)if(game_returnable(g,k)){g->selected_kind=k;break;}
    follow_captain(g,dt);
    if(g->stage){g->captain.y=stage_height(g->captain.x,g->captain.z,g->captain.y);g->other_captain.y=stage_height(g->other_captain.x,g->other_captain.z,g->other_captain.y);for(unsigned e=0;e<g->enemy_count;e++)g->enemies[e].pos.y=stage_height(g->enemies[e].pos.x,g->enemies[e].pos.z,g->enemies[e].pos.y);for(int i=0;i<SQUAD;i++)if(on_field(&g->pikis[i])&&g->pikis[i].state!=AIR&&g->pikis[i].state!=SEED_AIR&&g->pikis[i].state!=HELD&&g->pikis[i].state!=ATTACK&&g->pikis[i].state!=EXIT_ONION&&!(g->pikis[i].state==ENTER_ONION&&g->pikis[i].storage_phase))g->pikis[i].pos.y=stage_height(g->pikis[i].pos.x,g->pikis[i].pos.z,g->pikis[i].pos.y);}
    g->population=game_field_count(g);g->crate=g->items[0].pos;play_camera_step(g,in,dt);
}
Input game_tour(float time){(void)time;return (Input){0};}

void game_load_stage(Game *g,unsigned stage){
    Parameters p=g->parms;stage_select(stage);game_init(g,p);g->stage=stage;
    if(stage){
        float v[4]={0};stage_landmark(0,v);float a=v[3];g->bounds_x=4000;g->bounds_z=6700;
        g->captain=(V3){v[0]-40*cosf(a)+2*sinf(a),v[1],v[2]+40*sinf(a)+2*cosf(a)};
        g->other_captain=(V3){v[0]-60*cosf(a)-10*sinf(a),v[1],v[2]+60*sinf(a)-10*cosf(a)};g->face=g->other_face=a;
        float ox=g->captain.x,oz=g->captain.z+220;
        for(unsigned i=0;i<MAX_OBSTACLES;i++)g->obstacles[i].height=0;
        for(unsigned i=0;i<MAX_HAZARDS;i++){g->hazards[i].pos.x+=ox;g->hazards[i].pos.z+=oz;g->hazards[i].pos.y=stage_height(g->hazards[i].pos.x,g->hazards[i].pos.z,0);}
        for(unsigned i=0;i<g->item_count;i++){g->items[i].pos.x+=ox;g->items[i].pos.z+=oz;g->items[i].pos.y=stage_height(g->items[i].pos.x,g->items[i].pos.z,0);}
        for(unsigned i=0;i<g->posy_count;i++){g->posies[i].pos.x+=ox;g->posies[i].pos.z+=oz;g->posies[i].pos.y=stage_height(g->posies[i].pos.x,g->posies[i].pos.z,0);}
        for(unsigned i=0;i<SQUAD;i++){g->pikis[i].pos.x+=ox;g->pikis[i].pos.z+=oz;g->pikis[i].pos.y=stage_height(g->pikis[i].pos.x,g->pikis[i].pos.z,0);}
        for(unsigned i=0;i<g->enemy_count;i++){g->enemies[i].pos.x+=ox;g->enemies[i].pos.z+=oz;g->enemies[i].pos.y=stage_height(g->enemies[i].pos.x,g->enemies[i].pos.z,0);g->enemies[i].home=g->enemies[i].pos;}
        g->captain.y=stage_height(g->captain.x,g->captain.z,0);g->other_captain.y=stage_height(g->other_captain.x,g->other_captain.z,0);play_camera_init(g);
    }
}

bool game_ai_work(void *opaque,unsigned index,bool check){Game *g=opaque;if(index>=SQUAD)return false;Piki saved=g->pikis[index];work(g,&g->pikis[index]);bool found=g->pikis[index].state!=saved.state;if(check)g->pikis[index]=saved;return found;}
void game_ai_idle(void *opaque,unsigned index,bool bored){Game *g=opaque;if(index<SQUAD&&g->pikis[index].state==IDLE){g->pikis[index].vel=(V3){0};g->pikis[index].settle=bored?-1:0;}}
