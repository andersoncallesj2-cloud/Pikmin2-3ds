/* New native NDSP adapter, not the original JAudio engine. */
#include "audio.h"
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#define FRAMES 2048
typedef struct {FILE *file;u32 rate,frames,start,end,position;unsigned channel;bool loop;ndspWaveBuf wave[3];s16 *buffers[3];} Stream;
static Stream streams[12];static bool ready,legacy;static unsigned effect_next=2;static unsigned loop_sid[2];static s16 *legacy_data[12];static int legacy_channel[12];static bool legacy_paused;static u64 legacy_pause_start,legacy_end[12];static const char *status="not initialized";
static void close_stream(Stream *s){
    ndspChnWaveBufClear(s->channel);
    if(s->file){fclose(s->file);s->file=NULL;}
    memset(s->wave,0,sizeof(s->wave));
}
static bool open_stream(Stream *s,const char *name,bool loop){
    close_stream(s);char path[80];snprintf(path,sizeof(path),"romfs:/%s.p2s",name);s->file=fopen(path,"rb");if(!s->file)return false;
    u32 h[6];if(fread(h,1,24,s->file)!=24||memcmp(h,"P2S1",4)||h[2]!=2||h[1]<8000||h[1]>48000||!h[3]||h[5]>h[3]){close_stream(s);return false;}
    s->rate=h[1];s->frames=h[3];s->start=h[4];s->end=h[5]?h[5]:h[3];s->position=0;s->loop=loop;
    ndspChnReset(s->channel);ndspChnSetInterp(s->channel,NDSP_INTERP_LINEAR);ndspChnSetRate(s->channel,s->rate);ndspChnSetFormat(s->channel,NDSP_FORMAT_STEREO_PCM16);
    return true;
}
static void fill(Stream *s){
    if(!s->file)return;
    for(unsigned i=0;i<3;i++)if(s->wave[i].status==NDSP_WBUF_DONE||s->wave[i].status==NDSP_WBUF_FREE){
        unsigned n=0;
        while(n<FRAMES){
            if(s->position>=s->end){if(!s->loop)break;s->position=s->start;if(fseek(s->file,24+s->position*4,SEEK_SET))break;}
            unsigned count=FRAMES-n;if(count>s->end-s->position)count=s->end-s->position;
            unsigned got=fread(s->buffers[i]+n*2,4,count,s->file);s->position+=got;n+=got;if(got!=count)break;
        }
        if(!n)continue;
        DSP_FlushDataCache(s->buffers[i],n*4);s->wave[i]=(ndspWaveBuf){.data_vaddr=s->buffers[i],.nsamples=n};ndspChnWaveBufAdd(s->channel,&s->wave[i]);
    }
}
static bool legacy_play(const char *name,unsigned slot,bool loop){
    int channel=legacy_channel[slot];if(channel<0)return false;
    CSND_SetPlayState(channel,0);csndExecCmds(true);if(legacy_data[slot]){linearFree(legacy_data[slot]);legacy_data[slot]=NULL;}
    char path[80];snprintf(path,sizeof(path),"romfs:/%s.p2s",name);FILE *f=fopen(path,"rb");if(!f)return false;u32 h[6];
    if(fread(h,1,24,f)!=24||memcmp(h,"P2S1",4)||h[2]!=2||!h[3]||h[3]>48000*120){fclose(f);return false;}
    s16 *pcm=linearAlloc(h[3]*2);if(!pcm){fclose(f);return false;}s16 block[2048];unsigned done=0;
    while(done<h[3]){unsigned n=h[3]-done;if(n>1024)n=1024;if(fread(block,4,n,f)!=n){linearFree(pcm);fclose(f);return false;}for(unsigned i=0;i<n;i++)pcm[done+i]=((int)block[2*i]+block[2*i+1])/2;done+=n;}
    fclose(f);legacy_end[slot]=loop?~(u64)0:osGetTime()+h[3]*1000ULL/h[1];legacy_data[slot]=pcm;CSND_FlushDataCache(pcm,h[3]*2);
    Result rc=csndPlaySound(channel,SOUND_FORMAT_16BIT|(loop?SOUND_REPEAT:SOUND_ONE_SHOT),h[1],slot==0?.35f:.8f,0,pcm,pcm+(loop?h[4]:0),h[3]*2);return R_SUCCEEDED(rc);
}
bool audio_init(void){
    Result rc=ndspInit();if(R_FAILED(rc)){
        if(R_FAILED(csndInit())){status="Need /3ds/dspfirm.cdc; CSND unavailable";return false;}
        legacy=true;ready=true;unsigned n=0;for(int c=0;c<32&&n<12;c++)if(csndChannels&(1u<<c))legacy_channel[n++]=c;while(n<12)legacy_channel[n++]=-1;
        status="CSND fallback / no DSP firmware";legacy_play("music",0,true);return true;
    }ready=true;
    for(unsigned c=0;c<12;c++){Stream *s=&streams[c];s->channel=c;for(unsigned i=0;i<3;i++){s->buffers[i]=linearAlloc(FRAMES*4);if(!s->buffers[i]){status="audio allocation failed";audio_exit();return false;}}}
    if(!open_stream(&streams[0],"music",true)){status="music asset unavailable";return false;}
    status="NDSP / original title music";audio_update(false);return true;
}
void audio_update(bool paused){
    if(!ready)return;
    if(legacy){if(paused!=legacy_paused){u64 now=osGetTime();if(paused)legacy_pause_start=now;for(unsigned i=0;i<12;i++)if(legacy_channel[i]>=0&&legacy_end[i]>legacy_pause_start){if(!paused&&legacy_end[i]!=~(u64)0)legacy_end[i]+=now-legacy_pause_start;CSND_SetPlayState(legacy_channel[i],!paused);}csndExecCmds(false);legacy_paused=paused;}return;}
    for(unsigned c=0;c<12;c++){ndspChnSetPaused(c,paused);if(!paused)fill(&streams[c]);}
    bool jingle=ndspChnIsPlaying(1);float mix[12]={0};mix[0]=mix[1]=jingle?.15f:.45f;ndspChnSetMix(0,mix);mix[0]=mix[1]=.8f;ndspChnSetMix(1,mix);ndspChnSetMix(2,mix);
}
void audio_jingle(unsigned kind){
    if(!ready||kind>2)return;
    const char *names[]={"treasure-jingle","captain-down","pikmin-greeting"};if(legacy){legacy_play(names[kind],1,false);return;}open_stream(&streams[1],names[kind],false);fill(&streams[1]);
}
void audio_effect(unsigned kind){if(!ready||kind>2)return;const char *names[]={"throw","switch-olimar","switch-louie"};if(legacy){legacy_play(names[kind],2,false);return;}open_stream(&streams[2],names[kind],false);fill(&streams[2]);}
void audio_exit(void){if(!ready)return;if(legacy){for(unsigned i=0;i<12;i++)if(legacy_channel[i]>=0)CSND_SetPlayState(legacy_channel[i],0);csndExecCmds(true);csndExit();for(unsigned i=0;i<12;i++)if(legacy_data[i])linearFree(legacy_data[i]);ready=false;return;}for(unsigned c=0;c<12;c++)close_stream(&streams[c]);ndspExit();for(unsigned c=0;c<12;c++)for(unsigned i=0;i<3;i++)if(streams[c].buffers[i]){linearFree(streams[c].buffers[i]);streams[c].buffers[i]=NULL;}ready=false;}
const char *audio_status(void){return status;}

void audio_sound(unsigned sid,bool loop,float volume){
    if(!ready)return;
    unsigned slot=loop?((sid==7||sid==0x9f)?10:11):effect_next++;if(effect_next>=10)effect_next=2;
    if(loop&&loop_sid[slot-10]==sid)return;
    if(loop)loop_sid[slot-10]=sid;
    char name[32];snprintf(name,sizeof(name),"sfx-%04x",sid);if(legacy){legacy_play(name,slot,loop);return;}
    if(open_stream(&streams[slot],name,loop)){float mix[12]={0};mix[0]=mix[1]=volume;ndspChnSetMix(slot,mix);fill(&streams[slot]);}
}
void audio_stop_loops(void){loop_sid[0]=loop_sid[1]=0;if(!ready)return;for(unsigned i=10;i<12;i++){if(legacy){if(legacy_channel[i]>=0)CSND_SetPlayState(legacy_channel[i],0);}else close_stream(&streams[i]);}if(legacy)csndExecCmds(false);}
