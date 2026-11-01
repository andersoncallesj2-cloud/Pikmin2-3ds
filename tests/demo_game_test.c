/* D03 assertions, updated but intentionally NOT run in this handoff.
 * Link game.c, camera.c, original_follow.c, original_math.c and port_math.c; include the
 * private build source folder for generated demo_settings.h for future host runs. */
#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static Parameters fixture(void){
    Parameters p={.move=160,.run=120,.walk=10,.gravity=560,.throw_height=72.5f,.landing_time=1,
        .call_max=90,.call_min=5,.call_time=.45f,.shake=10,.neutral=.1f,.cursor_threshold=.8f,
        .stick_clamp=.85f,.cursor_radius=100,.cursor_speed=300,.cursor_look_time=.85f,
        .enemy_health=750,.enemy_speed=20,.enemy_wake=1,.enemy_attack_radius=40,.enemy_damage=10,
        .treasure_weight=8,.treasure_max=15,.carcass_weight=10,.carcass_max=20,.treasure_value=90,
        .carcass_yield=12,.captain_max_health=100,.punch_damage=10,.punch_range=10};
    for(int captain=0;captain<2;captain++){
        CameraParameters *c=&p.camera[captain];
        for(int i=0;i<6;i++)c->preset[i]=(CameraPreset){600,i>=3?60:21,23,38,500,55,1,2500};
        c->close_distance=250;c->close_angle=12;c->close_fov=35;c->collision_radius=300;
        c->collision_speed=.005f;c->collision_height=5;c->no_collision_height=25;
        c->setting=.1f;c->rotation=.15f;c->follow=.6f;c->accel=.005f;c->max_speed=.9f;c->damp=.85f;
    }
    return p;
}
static void tick(Game *g,Input in,int count){for(int i=0;i<count;i++)game_step(g,in,1.f/60);}
int main(void){
    Parameters p=fixture();Game g;game_init(&g,p);
    tick(&g,(Input){.x=.2f},10);assert(fabsf(g.captain.x)<.01f&&g.cursor_offset.x>0);
    game_init(&g,p);tick(&g,(Input){.grab_pressed=true,.grab_held=true},1);
    assert(g.held_piki>=0&&game_count(&g,HELD)==1);
    tick(&g,(Input){.throw_pressed=true},1);assert(g.throws==1&&game_count(&g,AIR)==1);
    tick(&g,(Input){0},100);assert(g.landings==1);
    tick(&g,(Input){.whistle=true},40);assert(game_count(&g,FOLLOW)==30);
    game_init(&g,p);tick(&g,(Input){.zoom_pressed=true,.zoom_held=true},1);assert(g.camera.zoom==2);
    tick(&g,(Input){.zoom_held=true},70);assert(g.camera.close);
    tick(&g,(Input){0},1);assert(!g.camera.close);
    tick(&g,(Input){.angle_pressed=true},1);assert(g.camera.high==1);
    tick(&g,(Input){.switch_pressed=true},1);assert(g.active_captain==1);
    game_init(&g,p);g.captain.z=-100;tick(&g,(Input){.x=1},600);assert(g.captain.x<=611.51f);
    game_init(&g,p);assert(game_field_count(&g)==30&&game_total_count(&g)==90);
    for(unsigned i=0;i<g.enemy_count;i++)g.enemies[i].delivered=g.enemies[i].dead=true;
    for(int i=0;i<8;i++){g.pikis[i].state=CARRY;g.pikis[i].item=0;g.pikis[i].pos=g.items[0].pos;}
    tick(&g,(Input){0},3000);assert(g.items[0].delivered&&g.pokos==90);
    game_init(&g,p);g.captain=game_onion_position(1);
    tick(&g,(Input){.grab_pressed=true},1);assert(g.onion_menu==1);
    tick(&g,(Input){.menu_delta=10},1);tick(&g,(Input){.menu_accept=true},1);
    assert(g.onion_stored[1]==10&&game_field_count(&g)==40);
    puts("D03 controller and destination assertions passed");return 0;
}
