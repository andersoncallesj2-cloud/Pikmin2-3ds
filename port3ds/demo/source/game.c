/* Native test-map controller. Original references (CC0 projectPiki):
 * Navi::{reviseController,makeVelocity,throwPiki,makeCStick}, NaviWhistle::update,
 * FakePiki movement, ActFormation settling and NaviFollowState normal movement.
 * World placement, local navigation/combat/storage FSMs below are adapters,
 * not the complete original Brain/ActTransport/EnemyFSM engine. */
#include "game.h"
#include "port_math.h"
#include "original_follow.h"
#include "demo_settings.h" /* Original tuning extracted by the private builder. */
#include <math.h>
#include <string.h>
#define PI 3.141592654f
static float length(float x,float z){return sqrtf(x*x+z*z);}
static float distance(V3 a,V3 b){return length(a.x-b.x,a.z-b.z);}
static float clamp(float x,float a,float b){return fmaxf(a,fminf(b,x));}
static bool on_field(const Piki *p){return p->state!=UNUSED&&p->state!=LOST&&p->state!=DYING;}
V3 game_onion_position(unsigned kind){return (V3){-180+(kind%3)*120.f,0,-300};}
V3 game_ship_position(void){return (V3){230,0,-320};}
bool game_in_water(V3 p){return p.x>100&&p.x<380&&p.z>175&&p.z<425;}
float game_day_ratio(const Game *g){return fmodf(g->time,DEMO_DAY_SECONDS)/DEMO_DAY_SECONDS;}
unsigned game_day_number(const Game *g){return 1+(unsigned)(g->time/DEMO_DAY_SECONDS);}
unsigned game_count(const Game *g,PikiState s){unsigned n=0;for(int i=0;i<SQUAD;i++)n+=g->pikis[i].state==s;return n;}
unsigned game_field_count(const Game *g){return SQUAD-game_count(g,UNUSED)-game_count(g,LOST)-game_count(g,DYING);}
unsigned game_total_count(const Game *g){return game_field_count(g)+g->onion_stored[0]+g->onion_stored[1]+g->onion_stored[2];}
unsigned game_squad_count(const Game *g,unsigned kind){
    unsigned n=0;for(int i=0;i<SQUAD;i++){const Piki *p=&g->pikis[i];n+=(p->state==FOLLOW||p->state==HELD)&&p->owner==g->active_captain&&(kind>=3||p->kind==kind);}return n;
}
unsigned game_returnable(const Game *g,unsigned kind){unsigned n=0;for(int i=0;i<SQUAD;i++){const Piki *p=&g->pikis[i];n+=p->state==FOLLOW&&p->owner==g->active_captain&&p->kind==kind;}return n;}
static unsigned assigned(const Game *g,int item){unsigned n=0;for(int i=0;i<SQUAD;i++)n+=g->pikis[i].state==CARRY&&g->pikis[i].item==item;return n;}
unsigned game_carriers(const Game *g,int item){
    if(item<0||(unsigned)item>=g->item_count)return 0;
    unsigned n=0;const CarryItem *o=&g->items[item];
    for(int i=0;i<SQUAD;i++)n+=g->pikis[i].state==CARRY&&g->pikis[i].item==item&&distance(g->pikis[i].pos,o->pos)<o->radius+25;
    return n;
}
static Piki *vacancy(Game *g){for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==UNUSED||g->pikis[i].state==LOST)return &g->pikis[i];return NULL;}
static void seed_birth(Game *g,unsigned kind,unsigned count){
    V3 home=game_onion_position(kind);
    for(unsigned i=0;i<count;i++){
        Piki *p=vacancy(g);if(!p){g->onion_stored[kind]+=count-i;break;}
        unsigned slot=(unsigned)(p-g->pikis);float angle=slot*2.39996323f+g->time;
        *p=(Piki){.pos={home.x,50,home.z},.vel={45*sinf(angle),90,45*cosf(angle)},.state=SEED_AIR,.kind=kind,.owner=g->active_captain,.item=-1,.target=-1};
    }
}
void game_init(Game *g,Parameters p){
    memset(g,0,sizeof(*g));g->parms=p;g->captain=(V3){0,0,-220};g->other_captain=(V3){28,0,-225};
    g->held_piki=-1;g->onion_menu=-1;g->selected_kind=1;
    g->cursor_offset=(V3){0,0,p.cursor_radius*.5f};g->other_cursor=g->cursor_offset;
    g->cursor=(V3){g->captain.x,0,g->captain.z+g->cursor_offset.z};
    g->captain_health[0]=g->captain_health[1]=p.captain_max_health;
    for(int i=0;i<SQUAD;i++)g->pikis[i]=(Piki){.state=UNUSED,.item=-1,.target=-1};
    for(int i=0;i<30;i++)g->pikis[i]=(Piki){.pos={(i%5-2)*11.f,0,-245-(i/5)*11.f},.state=FOLLOW,.kind=i%3,.owner=0,.item=-1,.target=-1};
    for(int i=0;i<3;i++)g->onion_stored[i]=20;
    const Obstacle rocks[MAX_OBSTACLES]={{{0,0,80},35,60,55},{{-300,0,150},25,60,70},{{400,0,-80},40,35,55},{{-430,0,-260},45,45,50},{{70,0,480},50,25,45},{{-300,0,430},30,30,50}};
    memcpy(g->obstacles,rocks,sizeof(rocks));
    const Hazard hazards[MAX_HAZARDS]={{{-300,0,30},28,0},{{-450,0,320},28,0},{{-150,0,400},28,0},{{400,0,0},28,0},{{-100,0,170},29,1},{{80,0,400},29,1},{{430,0,150},29,1}};
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
    p->x=clamp(p->x,-620+r,620-r);p->z=clamp(p->z,-650+r,650-r);
    for(unsigned i=0;i<MAX_OBSTACLES;i++){
        const Obstacle *b=&g->obstacles[i];float dx=p->x-b->pos.x,dz=p->z-b->pos.z,ex=b->half_x+r,ez=b->half_z+r;
        if(p->y<b->height&&fabsf(dx)<ex&&fabsf(dz)<ez){if(ex-fabsf(dx)<ez-fabsf(dz))p->x=b->pos.x+copysignf(ex,dx);else p->z=b->pos.z+copysignf(ez,dz);}
    }
}
static V3 waypoint(const Game *g,V3 from,V3 to,float r){
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
    V3 t=waypoint(g,p->pos,goal,4);float dx=t.x-p->pos.x,dz=t.z-p->pos.z,d=length(dx,dz);speed=fminf(speed,d/dt);
    p->vel.x=d>.2f?dx/d*speed:0;p->vel.z=d>.2f?dz/d*speed:0;p->pos.x+=p->vel.x*dt;p->pos.z+=p->vel.z*dt;
    if(d>1)p->face=roundAng(p->face+fminf(1,dt*8)*angDist(roundAng(atan2f(dx,dz)),p->face));
    collide(g,&p->pos,4);
}
static void die(Game *g,Piki *p){if(on_field(p)){p->state=DYING;p->work_time=0;p->pos.y=0;p->item=-1;g->deaths++;}}
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
        g->pikis[i].state=FOLLOW;g->pikis[i].owner=g->active_captain;g->pikis[i].pos.y=0;g->plucking=true;g->pluck_timer=.35f;return true;
    }
    return false;
}
static void onion_menu(Game *g,Input *in){
    unsigned color=(unsigned)g->onion_menu;
    int capacity=(int)(SQUAD-game_field_count(g)),take=(int)g->onion_stored[color];if(take>capacity)take=capacity;
    g->onion_adjust=(int)clamp(g->onion_adjust+in->menu_delta,-(float)game_returnable(g,color),(float)take);
    if(in->menu_accept){
        if(g->onion_adjust>0)for(int n=0;n<g->onion_adjust;n++){
            Piki *p=vacancy(g);if(!p)break;V3 home=game_onion_position(color);
            *p=(Piki){.pos={home.x,22,home.z},.vel={0,100,60},.state=AIR,.kind=color,.owner=g->active_captain,.item=-1,.target=-1,.rejoin=true};g->onion_stored[color]--;
        }
        else{int remaining=-g->onion_adjust;for(int i=0;i<SQUAD&&remaining;i++){Piki *p=&g->pikis[i];if(p->state==FOLLOW&&p->owner==g->active_captain&&p->kind==color){p->state=ENTER_ONION;p->item=(int)color;remaining--;}}}
        g->onion_menu=-1;g->onion_adjust=0;
    }else if(in->menu_cancel){g->onion_menu=-1;g->onion_adjust=0;}
    in->x=in->z=0;in->grab_pressed=in->throw_pressed=in->whistle=false;
}
static void throw_piki(Game *g){
    if(g->held_piki<0)return;
    Piki *p=&g->pikis[g->held_piki];p->state=AIR;p->rejoin=false;
    p->pos=(V3){g->captain.x-15*sinf(g->face),10,g->captain.z-15*cosf(g->face)};
    float height=p->kind==2?DEMO_YELLOW_THROW:g->parms.throw_height,half=.5f*g->parms.landing_time;
    float vy=half*(.5f*g->parms.gravity)+height/half,time=2*vy/g->parms.gravity;
    p->vel=(V3){(g->cursor.x-p->pos.x)/time+g->velocity.x,vy,(g->cursor.z-p->pos.z)/time+g->velocity.z};g->throws++;g->held_piki=-1;g->captain_action=5;g->action_timer=.5f;
}
static void elemental_hazards(Game *g,Piki *p,float dt){
    if(!on_field(p)||p->state==SPROUT||p->state==SEED_AIR||p->state==HELD||p->pos.y>20)return;
    for(unsigned i=0;i<MAX_HAZARDS;i++)if(distance(p->pos,g->hazards[i].pos)<g->hazards[i].radius){
        if(g->hazards[i].kind==1&&p->kind!=2){die(g,p);return;}
        if(g->hazards[i].kind==0&&p->kind!=1&&p->state!=BURNING){p->state=BURNING;p->work_time=0;p->item=-1;p->target=(int)i;}
    }
    if(game_in_water(p->pos)&&p->kind!=0&&p->state!=DROWNING){p->state=DROWNING;p->work_time=0;p->item=-1;p->called=false;}
    if(p->state==BURNING){
        Hazard *h=&g->hazards[p->target];float a=atan2f(p->pos.x-h->pos.x,p->pos.z-h->pos.z);
        V3 escape={p->pos.x+50*sinf(a),0,p->pos.z+50*cosf(a)};walk(g,p,escape,g->parms.run*1.3f,dt);p->work_time+=dt;if(p->work_time>DEMO_FIRE_TIME)die(g,p);
    }else if(p->state==DROWNING){
        p->work_time+=dt;
        if(p->called)walk(g,p,g->captain,g->parms.walk+(g->parms.run-g->parms.walk)*.5f,dt);
        if(!game_in_water(p->pos)){p->state=FOLLOW;p->owner=g->active_captain;p->pos.y=0;}
        else if(p->work_time>10)die(g,p);
    }else if(p->state==FOLLOW||p->state==IDLE||p->state==CARRY||p->state==ENTER_ONION)p->pos.y=game_in_water(p->pos)?-4:0;
}
static void enemy_step(Game *g,unsigned id,float dt){
    Bulborb *e=&g->enemies[id];const float *t=demo_enemy_tuning[e->kind];if(e->dead)return;
    if(e->health<=0){e->dead=true;e->state=3;g->items[e->item].pos=e->pos;g->items[e->item].available=true;return;}
    e->cooldown=fmaxf(0,e->cooldown-dt);V3 target=g->captain;float nearest=distance(e->pos,target);
    for(int i=0;i<SQUAD;i++)if(on_field(&g->pikis[i])&&g->pikis[i].state!=ATTACK&&g->pikis[i].state!=SPROUT&&g->pikis[i].pos.y<25){float d=distance(e->pos,g->pikis[i].pos);if(d<nearest){nearest=d;target=g->pikis[i].pos;}}
    if(e->state==0&&nearest>t[2]&&e->health>=e->max_health)return;
    e->state=1;if(nearest>240||distance(e->home,e->pos)>220)target=e->home;
    float angle=roundAng(atan2f(target.x-e->pos.x,target.z-e->pos.z));e->face=roundAng(e->face+fminf(1,dt*2)*angDist(angle,e->face));
    if(nearest>t[3]*.75f){
        V3 next=waypoint(g,e->pos,target,t[8]*.5f);float d=distance(e->pos,next);
        if(d>1){e->pos.x+=(next.x-e->pos.x)/d*t[1]*dt;e->pos.z+=(next.z-e->pos.z)/d*t[1]*dt;collide(g,&e->pos,t[8]*.5f);}
    }else if(e->cooldown<=0){
        e->state=2;e->cooldown=e->kind==2?1.3f:1.8f;int eaten=0;
        for(int i=0;i<SQUAD&&eaten<(e->kind==2?1:2);i++){
            Piki *p=&g->pikis[i];float a=atan2f(p->pos.x-e->pos.x,p->pos.z-e->pos.z);
            if(on_field(p)&&p->state!=HELD&&p->state!=ATTACK&&p->state!=SPROUT&&p->pos.y<25&&distance(p->pos,e->pos)<t[3]&&cosf(a-e->face)>.2f){die(g,p);eaten++;}
        }
        if(distance(g->captain,e->pos)<t[3])g->captain_health[g->active_captain]=fmaxf(0,g->captain_health[g->active_captain]-t[4]);
    }
}
static void follow_captain(Game *g,float dt){
    if(!g->other_following||g->captain_health[g->active_captain^1]<=0)return;
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
    V3 centers[3]={{0}};unsigned counts[3]={0},rank[3]={0};float radius[3]={0};
    for(int i=0;i<SQUAD;i++){Piki *p=&g->pikis[i];if(p->state==FOLLOW&&p->owner==g->active_captain){centers[p->kind].x+=p->pos.x;centers[p->kind].z+=p->pos.z;counts[p->kind]++;}}
    for(unsigned c=0;c<3;c++)if(counts[c]){centers[c].x/=counts[c];centers[c].z/=counts[c];radius[c]=sqrtf(counts[c])*6.25f;}
    for(unsigned pass=0;pass<4;pass++)for(unsigned c=0;c<3;c++)if(counts[c]){
        V3 captains[]={g->captain,g->other_captain};
        for(unsigned n=0;n<2;n++){float dx=centers[c].x-captains[n].x,dz=centers[c].z-captains[n].z,d=length(dx,dz),min=radius[c]+45;if(d<min){float a=g->face+c*2*PI/3;if(d<.01f){dx=sinf(a);dz=cosf(a);d=1;}centers[c].x+=dx/d*(min-d);centers[c].z+=dz/d*(min-d);}}
        for(unsigned j=c+1;j<3;j++)if(counts[j]){float dx=centers[c].x-centers[j].x,dz=centers[c].z-centers[j].z,d=length(dx,dz),min=radius[c]+radius[j]+20;if(d<min){if(d<.01f){dx=1;dz=0;d=1;}float push=(min-d);centers[c].x+=dx/d*push;centers[c].z+=dz/d*push;centers[j].x-=dx/d*push;centers[j].z-=dz/d*push;}}
    }
    for(int i=0;i<SQUAD;i++){Piki *p=&g->pikis[i];if(p->state==FOLLOW&&p->owner==g->active_captain){unsigned n=rank[p->kind]++;float a=n*2.39996323f,r=6.25f*sqrtf(n);p->disband_target=(V3){centers[p->kind].x+r*sinf(a),0,centers[p->kind].z+r*cosf(a)};collide(g,&p->disband_target,4);p->state=DISBANDING;p->item=-1;p->work_time=0;}}
    g->other_following=false;
}
void game_step(Game *g,Input in,float dt){
    if(in.reset){Parameters p=g->parms;game_init(g,p);return;}if(!(dt>0&&dt<=.05f))return;
    g->time+=dt;g->steps++;g->pluck_timer=fmaxf(0,g->pluck_timer-dt);g->action_timer=fmaxf(0,g->action_timer-dt);
    if(g->onion_menu>=0)onion_menu(g,&in);
    if(in.switch_pressed&&g->held_piki<0&&g->captain_health[g->active_captain^1]>0){
        V3 p=g->captain;g->captain=g->other_captain;g->other_captain=p;p=g->cursor_offset;g->cursor_offset=g->other_cursor;g->other_cursor=p;
        float f=g->face;g->face=g->other_face;g->other_face=f;g->velocity=(V3){0};g->other_velocity=(V3){0};g->active_captain^=1;
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
    g->cursor=(V3){g->captain.x+g->cursor_offset.x,0,g->captain.z+g->cursor_offset.z};g->aim_distance=length(g->cursor_offset.x,g->cursor_offset.z);
    if(in.dismiss)dismiss_colours(g);
    if(!in.grab_held)g->plucking=false;
    if(g->plucking&&in.grab_held&&g->pluck_timer<=0)pluck(g);
    if(in.grab_pressed&&g->held_piki<0&&g->onion_menu<0){
        if(pluck(g))in.grab_pressed=false;
        else for(unsigned kind=0;kind<3;kind++)if(distance(g->captain,game_onion_position(kind))<45){g->onion_menu=(int)kind;g->onion_adjust=0;in.grab_pressed=false;break;}
        if(in.grab_pressed){
            for(int pass=0;pass<2&&g->held_piki<0;pass++)for(int i=0;i<SQUAD;i++){
                Piki *p=&g->pikis[i];if(p->state==FOLLOW&&p->owner==g->active_captain&&(pass||p->kind==g->selected_kind)&&distance(p->pos,g->captain)<85){g->held_piki=i;p->state=HELD;g->selected_kind=p->kind;break;}
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
        for(unsigned pass=1;pass<=3;pass++){
            unsigned kind=(start+pass)%3;int found=-1;for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==FOLLOW&&g->pikis[i].owner==g->active_captain&&g->pikis[i].kind==kind&&distance(g->pikis[i].pos,g->captain)<85){found=i;break;}
            if(found>=0){g->pikis[g->held_piki].state=FOLLOW;g->held_piki=found;g->pikis[found].state=HELD;g->selected_kind=kind;break;}
        }
    }
    if(in.throw_pressed||(g->held_piki>=0&&!in.grab_held))throw_piki(g);
    if(g->held_piki>=0)g->pikis[g->held_piki].pos=(V3){g->captain.x+8*sinf(g->face),25,g->captain.z+8*cosf(g->face)};
    g->calling=in.whistle&&g->held_piki<0&&g->onion_menu<0;
    if(g->calling){
        g->whistle_time=fminf(g->parms.call_time,g->whistle_time+dt);g->whistle_radius=g->parms.call_min+(g->parms.call_max-g->parms.call_min)*g->whistle_time/g->parms.call_time;
        for(int i=0;i<SQUAD;i++){
            Piki *p=&g->pikis[i];if(!on_field(p)||p->state==AIR||p->state==HELD||p->state==SPROUT||p->state==SEED_AIR)continue;
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
            if(!seed&&!p->rejoin)for(unsigned e=0;e<g->enemy_count;e++)if(!g->enemies[e].dead&&distance(p->pos,g->enemies[e].pos)<demo_enemy_tuning[g->enemies[e].kind][8]&&p->pos.y<demo_enemy_tuning[g->enemies[e].kind][9]+40){p->state=ATTACK;p->target=(int)e;p->work_time=0;g->landings++;break;}
            if((p->state==AIR||p->state==SEED_AIR)&&p->pos.y<=0){p->pos.y=0;p->vel=(V3){0};if(seed)p->state=SPROUT;else{p->state=p->rejoin?FOLLOW:IDLE;g->landings++;if(!p->rejoin)work(g,p);}}
        }else if(p->state==FOLLOW){
            bool active=p->owner==g->active_captain;V3 leader=active?g->captain:g->other_captain;float face=active?g->face:g->other_face;
            unsigned rank=ranks[p->owner%2]++;float side=((int)(rank%7)-3)*10.f,behind=24+(rank/7)*11.f;
            V3 target={leader.x+cosf(face)*side-sinf(face)*behind,0,leader.z-sinf(face)*side-cosf(face)*behind};
            if(active&&g->swarm_strength>0){float forward=55+g->swarm_strength*65+(rank/7)*9;target=(V3){leader.x+cosf(g->swarm_angle)*side+sinf(g->swarm_angle)*forward,0,leader.z-sinf(g->swarm_angle)*side+cosf(g->swarm_angle)*forward};}
            float dist=distance(p->pos,target);if(dist<=7)p->settle=0;else if(dist<15)p->settle=fminf(6,p->settle+dt*30);else p->settle=0;
            if(dist<=7||(dist<=15&&p->settle<6))p->vel=(V3){0};else walk(g,p,target,dist<15?g->parms.walk:g->parms.run,dt);
            if(active&&g->swarm_strength>0&&distance(p->pos,target)<35)work(g,p);
        }else if(p->state==DISBANDING){walk(g,p,p->disband_target,g->parms.run,dt);p->work_time+=dt;if(distance(p->pos,p->disband_target)<5||p->work_time>5){p->state=IDLE;p->vel=(V3){0};}}
        else if(p->state==IDLE){p->pos.y=0;work(g,p);}
        else if(p->state==ENTER_ONION){V3 home=game_onion_position(p->kind);walk(g,p,home,g->parms.run,dt);if(distance(p->pos,home)<7){g->onion_stored[p->kind]++;p->state=UNUSED;}}
        else if(p->state==ATTACK){
            bool dead;V3 target;float radius,height;
            if(p->target>=0){Bulborb *e=&g->enemies[p->target];dead=e->dead;target=e->pos;radius=demo_enemy_tuning[e->kind][8]*.6f;height=demo_enemy_tuning[e->kind][9]*.5f;}
            else{PelletPosy *plant=&g->posies[-p->target-1];dead=!plant->alive;target=plant->pos;radius=12;height=0;}
            if(dead){p->state=IDLE;p->pos.y=0;work(g,p);}
            else{float a=i*2*PI/30;p->pos=(V3){target.x+radius*sinf(a),height,target.z+radius*cosf(a)};p->work_time+=dt;if(p->work_time>=.65f){p->work_time-=.65f;float damage=p->kind==1?15:10;if(p->target>=0)g->enemies[p->target].health-=damage;else g->posies[-p->target-1].health-=damage;}}
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
        unsigned n=assigned(g,item),slot=0,colors[3]={0,0,0};
        for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==CARRY&&g->pikis[i].item==(int)item){float a=slot++*2*PI/fmaxf(1,n),r=o->radius+10;V3 t={o->pos.x+r*sinf(a),0,o->pos.z+r*cosf(a)};walk(g,&g->pikis[i],t,g->parms.run,dt);if(distance(g->pikis[i].pos,o->pos)<o->radius+25)colors[g->pikis[i].kind]++;}
        unsigned carriers=colors[0]+colors[1]+colors[2],goal=o->destination;
        for(unsigned c=0;c<3;c++)if(colors[c]>colors[goal])goal=c;
        o->destination=goal;
        if(carriers>=(unsigned)o->weight){
            V3 destination=o->type==ITEM_TREASURE?game_ship_position():game_onion_position(goal);float dist=distance(o->pos,destination);
            if(dist<12){
                o->delivered=true;if(o->type==ITEM_TREASURE){g->delivered++;g->pokos+=o->value;}else seed_birth(g,goal,o->type==ITEM_PELLET&&goal==o->color?o->matching_seeds:o->seeds);
                for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==CARRY&&g->pikis[i].item==(int)item){g->pikis[i].state=IDLE;g->pikis[i].item=-1;}
            }else{
                V3 next=waypoint(g,o->pos,destination,o->radius+5);float d=distance(o->pos,next),speed=22+12*(carriers-o->weight)/fmaxf(1,o->max_carriers-o->weight);
                if(d>1){o->pos.x+=(next.x-o->pos.x)/d*speed*dt;o->pos.z+=(next.z-o->pos.z)/d*speed*dt;}o->lift=2;
            }
        }else o->lift=0;
    }
    for(unsigned e=0;e<g->enemy_count;e++)if(g->enemies[e].dead){g->enemies[e].pos=g->items[g->enemies[e].item].pos;g->enemies[e].delivered=g->items[g->enemies[e].item].delivered;}
    /* Local separation is new collision glue, not original CPlate code. */
    for(int i=0;i<SQUAD;i++)for(int j=0;j<i;j++){
        Piki *a=&g->pikis[i],*b=&g->pikis[j];if((a->state!=FOLLOW&&a->state!=IDLE)||(b->state!=FOLLOW&&b->state!=IDLE))continue;
        float dx=a->pos.x-b->pos.x,dz=a->pos.z-b->pos.z,d=length(dx,dz);if(d>.001f&&d<7){float push=(7-d)*.25f;a->pos.x+=dx/d*push;a->pos.z+=dz/d*push;b->pos.x-=dx/d*push;b->pos.z-=dz/d*push;collide(g,&a->pos,4);collide(g,&b->pos,4);}
    }
    if(g->held_piki<0&&!game_returnable(g,g->selected_kind))for(unsigned k=0;k<3;k++)if(game_returnable(g,k)){g->selected_kind=k;break;}
    follow_captain(g,dt);g->population=game_field_count(g);g->crate=g->items[0].pos;play_camera_step(g,in,dt);
}
Input game_tour(float time){(void)time;return (Input){0};}
