/* Adapted from projectPiki PlayCamera, 999140f3521e22619547d5d61862a6577ea73882,
 * src/plugProjectNishimuraU/playCamera.cpp (CC0). Native C data access replaces
 * Navi, CPlate and MapMgr. Camera presets are read from the user's disc.
 * Per-frame interpolation is converted from 30 Hz to the native fixed step.
 * Vibration, caves, movies and original map collision are not ported here. */
#include "game.h"
#include "port_math.h"
#include <math.h>
static float radians(float d){return d*.01745329252f;}
static float rate(float r,float dt){return 1-powf(1-r,dt*30);}
static CameraPreset target(const Game *g){
    const CameraParameters *p=&g->parms.camera[g->active_captain];
    if(g->camera.close)return (CameraPreset){p->close_distance,p->close_angle,p->close_fov,10,1000,27.5f,1,12800};
    return p->preset[g->camera.high*3+g->camera.zoom];
}
static V3 goal_position(const Game *g,CameraPreset p){
    V3 pos={g->captain.x*p.weight,g->captain.y*p.weight,g->captain.z*p.weight};float weight=p.weight;
    for(int i=0;i<SQUAD;i++)if(g->pikis[i].state==FOLLOW){
        pos.x+=g->pikis[i].pos.x;pos.y+=g->pikis[i].pos.y;pos.z+=g->pikis[i].pos.z;weight++;
    }
    pos.x/=weight;pos.y/=weight;pos.z/=weight;
    float dx=pos.x-g->captain.x,dy=pos.y-g->captain.y,dz=pos.z-g->captain.z;
    float distance=sqrtf(dx*dx+dy*dy+dz*dz);
    if(distance>p.detached){float ratio=p.detached/distance;pos=(V3){g->captain.x+dx*ratio,g->captain.y+dy*ratio,g->captain.z+dz*ratio};}
    pos.y+=p.offset;return pos;
}
void play_camera_init(Game *g){
    PlayCamera *c=&g->camera;*c=(PlayCamera){.zoom=1};
    c->target_theta=c->theta=roundAng(g->face+3.141592654f);
    CameraPreset p=target(g);V3 pos=goal_position(g,p);
    c->distance=p.distance;c->phi=radians(p.angle);c->fov=p.fov;c->near_z=p.near_z;c->far_z=p.far_z;
    c->look_x=pos.x;c->look_y=pos.y;c->look_z=pos.z;g->yaw=roundAng(c->theta-3.141592654f);
}
void play_camera_step(Game *g,Input in,float dt){
    PlayCamera *c=&g->camera;const CameraParameters *p=&g->parms.camera[g->active_captain];
    /* PlayCamera::updateCameraMode/startGameCamera: R cycles three zooms;
     * holding it for one second enters close view. Z toggles two angles. */
    if(in.zoom_pressed&&!c->close){c->zoom=(c->zoom+1)%3;c->hold_r=0;c->collision_ready=0;}
    if(in.angle_pressed&&!c->close){c->high^=1;c->collision_ready=0;}
    if(in.zoom_held){c->hold_r+=dt;c->close=c->hold_r>=1;}else{c->hold_r=0;c->close=0;}
    if(in.center_pressed)c->follow_time=p->follow;
    else if(in.center_held){
        c->smooth+=(-in.x)*p->accel*(dt*30);
        c->smooth=fmaxf(-p->max_speed*dt,fminf(p->max_speed*dt,c->smooth));
    }
    /* changeTargetTheta: centring follows the CURSOR, not a guessed face angle. */
    if(c->follow_time>0){
        c->follow_time-=dt;
        if(g->cursor_offset.x*g->cursor_offset.x+g->cursor_offset.z*g->cursor_offset.z>1)
            c->target_theta=roundAng(atan2f(-g->cursor_offset.x,-g->cursor_offset.z));
    }else c->target_theta=roundAng(c->target_theta-c->smooth);
    c->smooth*=powf(p->damp,dt*30);
    CameraPreset preset=target(g);V3 pos=goal_position(g,preset);
    float phi=radians(preset.angle);
    /* Original 15-sample elevation correction, querying this demo's floor/box. */
    if(!c->high||c->close){
        float step=(c->close?p->close_distance:p->collision_radius)/15;
        for(int i=1;i<=15;i++){
            float radius=step*i,x=pos.x+sinf(c->target_theta)*radius*cosf(phi),z=pos.z+cosf(c->target_theta)*radius*cosf(phi);
            float floor=(fabsf(x-100)<25&&fabsf(z-25)<25)?35:0;
            float height=floor+i*p->collision_height;
            if(height>pos.y+radius*sinf(phi))phi=fmaxf(phi,atan2f(height-pos.y,radius));
        }
        if(!c->close&&c->collision_ready)phi=c->phi+(phi-c->phi)*rate(p->collision_speed,dt);
    }
    /* PlayCamera::updateParms, with original wrapped angular interpolation. */
    float r=rate(p->setting,dt),look=c->close?rate(.175f,dt):r;
    c->distance+=(preset.distance-c->distance)*r;c->phi+=(phi-c->phi)*r;c->fov+=(preset.fov-c->fov)*r;
    c->near_z+=(preset.near_z-c->near_z)*r;c->far_z+=(preset.far_z-c->far_z)*r;
    if(fabsf(c->phi-radians(preset.angle))<.1f)c->collision_ready=1;
    c->look_x+=(pos.x-c->look_x)*look;c->look_y+=(pos.y-c->look_y)*look;c->look_z+=(pos.z-c->look_z)*look;
    c->theta=roundAng(c->theta+rate(p->rotation,dt)*angDist(c->target_theta,c->theta));
    g->yaw=roundAng(c->theta-3.141592654f);
}
