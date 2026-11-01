/* Native frontend/navigation; original font and window art are drawn in main.c.
 * Not the complete original title/pause/J2D menu engine. */
#include "front.h"
#include <3ds.h>
#include <stdio.h>
Settings settings={0x50325335,false,false,false,true};
unsigned front_view=VIEW_TITLE,front_choice,debug_page,debug_entity;bool front_quit;float front_time;
static unsigned return_view=VIEW_TITLE;static float accept_delay;
bool front_title_scene(void){return front_view==VIEW_TITLE||front_view==VIEW_TITLE_MENU||front_view==VIEW_MAPS||front_view==VIEW_UNAVAILABLE||((front_view==VIEW_SETTINGS||front_view==VIEW_3DS)&&return_view!=VIEW_PAUSE);}
void front_init(void){FILE *f=fopen("sdmc:/3ds/pikmin2-demo/settings.bin","rb");if(f){Settings s;if(fread(&s,1,sizeof(s),f)==sizeof(s)&&s.magic==settings.magic)settings=s;fclose(f);}}
void front_save(void){FILE *f=fopen("sdmc:/3ds/pikmin2-demo/settings.bin","wb");if(f){fwrite(&settings,1,sizeof(settings),f);fclose(f);}}
static void enter(unsigned view){front_view=view;front_choice=0;front_time=0;accept_delay=.15f;}
bool front_update(Game *g,uint32_t down,uint32_t held,float dt){
    front_time+=dt;accept_delay-=dt;
    if(front_view==VIEW_TITLE_MENU&&(down&KEY_SELECT))front_quit=true;
    if(front_view==VIEW_PLAY){
        if(g->game_over)enter(VIEW_GAME_OVER);
        else if(g->treasure_item>=0)enter(VIEW_TREASURE);
        else if(down&KEY_START)enter(VIEW_PAUSE);
        if(settings.debug&&(down&KEY_SELECT))debug_page=(debug_page+1)%5;
        if(settings.debug&&(held&KEY_SELECT)){if(down&KEY_DRIGHT)debug_entity=(debug_entity+1)%SQUAD;if(down&KEY_DLEFT)debug_entity=(debug_entity+SQUAD-1)%SQUAD;}
        return front_view==VIEW_PLAY;
    }
    if(front_view==VIEW_TITLE){if((down&(KEY_A|KEY_START))&&front_time>.5f)enter(VIEW_TITLE_MENU);return false;}
    unsigned count=front_view==VIEW_TITLE_MENU?7:front_view==VIEW_MAPS?3:front_view==VIEW_PAUSE?4:front_view==VIEW_SETTINGS?4:front_view==VIEW_3DS?4:1;
    if(down&KEY_DUP)front_choice=(front_choice+count-1)%count;
    if(down&KEY_DDOWN)front_choice=(front_choice+1)%count;
    if((down&KEY_B)||(down&KEY_START)){
        if(front_view==VIEW_PAUSE)enter(VIEW_PLAY);
        else if(front_view==VIEW_3DS)enter(VIEW_SETTINGS);
        else if(front_view==VIEW_SETTINGS)enter(return_view);
        else if(front_view==VIEW_MAPS||front_view==VIEW_UNAVAILABLE)enter(VIEW_TITLE_MENU);
        else if(front_view==VIEW_TITLE_MENU)enter(VIEW_TITLE);
    }
    if((down&KEY_A)&&accept_delay<=0){
        switch(front_view){
        case VIEW_TITLE_MENU:if(front_choice==0){game_load_stage(g,1);enter(VIEW_PLAY);}else if(front_choice==6)enter(VIEW_MAPS);else if(front_choice==3){return_view=VIEW_TITLE_MENU;enter(VIEW_SETTINGS);}else enter(VIEW_UNAVAILABLE);break;
        case VIEW_MAPS:if(front_choice<2){game_load_stage(g,front_choice);enter(VIEW_PLAY);}else enter(VIEW_TITLE_MENU);break;
        case VIEW_PAUSE:if(front_choice==0)enter(VIEW_PLAY);else if(front_choice==1){return_view=VIEW_PAUSE;enter(VIEW_SETTINGS);}else if(front_choice==2)enter(VIEW_TITLE);else front_quit=true;break;
        case VIEW_SETTINGS:if(front_choice==0)settings.audio=!settings.audio;else if(front_choice==1)settings.debug=!settings.debug;else if(front_choice==2)enter(VIEW_3DS);else enter(return_view);front_save();break;
        case VIEW_3DS:if(front_choice==0)settings.stereo=!settings.stereo;else if(front_choice==1)settings.hitboxes=!settings.hitboxes;else if(front_choice==3)enter(VIEW_SETTINGS);front_save();break;
        case VIEW_TREASURE:if(front_time>1){g->treasure_item=-1;enter(VIEW_PLAY);}break;
        case VIEW_GAME_OVER:enter(VIEW_TITLE);break;
        case VIEW_UNAVAILABLE:enter(VIEW_TITLE_MENU);break;
        }
    }
    return false;
}
