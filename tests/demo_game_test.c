#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static Parameters parms={160,120,10,560,72.5f,1,90,5,.45f};
static void tick(Game *g,Input in,int n){for(int i=0;i<n;i++)game_step(g,in,1.f/60);}
int main(void){
    Game g;game_init(&g,parms);
    tick(&g,(Input){.throw_pressed=true},1);
    assert(game_count(&g,AIR)==1&&g.throws==1);float peak=0;
    for(int i=0;i<90;i++){tick(&g,(Input){0},1);if(g.pikis[0].pos.y>peak)peak=g.pikis[0].pos.y;}
    assert(peak>79&&peak<84&&g.landings==1);assert(game_count(&g,CARRY)==1);
    tick(&g,(Input){.whistle=true},40);assert(g.recalls==1&&game_count(&g,FOLLOW)==5);
    tick(&g,(Input){.dismiss=true},1);assert(game_count(&g,IDLE)==5);
    game_init(&g,parms);g.captain.z=-100;tick(&g,(Input){.x=1},600);assert(g.captain.x<=231.51f&&g.captain.x>220);
    game_init(&g,parms);g.captain=(V3){100,0,-30};tick(&g,(Input){.z=1},20);assert(g.captain.z<=-8.49f);
    game_init(&g,parms);for(int i=0;i<3;i++)g.pikis[i].pos=g.crate;
    tick(&g,(Input){.dismiss=true},1);assert(game_count(&g,CARRY)==3);
    tick(&g,(Input){0},900);assert(g.delivered==1&&game_count(&g,CARRY)==0);
    game_init(&g,parms);for(int i=0;i<1800;i++)game_step(&g,game_tour(g.time),1.f/60);
    assert(g.throws==3&&g.landings==3&&g.delivered==1);
    printf("PASS: throw trajectory, landing, whistle, dismiss, arena/block collision, carry/delivery, tour\n");return 0;
}
