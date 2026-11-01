/* New native NDSP adapter, not the original JAudio engine. */
#include "audio.h"
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#define FRAMES 2048
typedef struct {FILE *file;u32 rate,frames,start,end,position;unsigned channel;bool loop;ndspWaveBuf wave[3];s16 *buffers[3];} Stream;
static Stream streams[2];static bool ready;static const char *status="not initialized";
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
bool audio_init(void){
    Result rc=ndspInit();if(R_FAILED(rc)){status="DSP unavailable (dspfirm.cdc)";return false;}ready=true;
    for(unsigned c=0;c<2;c++){Stream *s=&streams[c];s->channel=c;for(unsigned i=0;i<3;i++){s->buffers[i]=linearAlloc(FRAMES*4);if(!s->buffers[i]){status="audio allocation failed";audio_exit();return false;}}}
    if(!open_stream(&streams[0],"music",true)){status="music asset unavailable";return false;}
    status="NDSP / original title music";audio_update(false);return true;
}
void audio_update(bool paused){
    if(!ready)return;
    for(unsigned c=0;c<2;c++){ndspChnSetPaused(c,paused);if(!paused)fill(&streams[c]);}
    bool jingle=ndspChnIsPlaying(1);float mix[12]={0};mix[0]=mix[1]=jingle?.15f:.45f;ndspChnSetMix(0,mix);mix[0]=mix[1]=.8f;ndspChnSetMix(1,mix);
}
void audio_jingle(unsigned kind){
    if(!ready||kind>2)return;
    const char *names[]={"treasure-jingle","captain-down","pikmin-greeting"};open_stream(&streams[1],names[kind],false);fill(&streams[1]);
}
void audio_exit(void){if(!ready)return;for(unsigned c=0;c<2;c++)close_stream(&streams[c]);ndspExit();for(unsigned c=0;c<2;c++)for(unsigned i=0;i<3;i++)if(streams[c].buffers[i]){linearFree(streams[c].buffers[i]);streams[c].buffers[i]=NULL;}ready=false;}
const char *audio_status(void){return status;}
