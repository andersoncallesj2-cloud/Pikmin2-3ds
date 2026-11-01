#ifndef DEMO_AUDIO_H
#define DEMO_AUDIO_H
#include <stdbool.h>
bool audio_init(void);
void audio_update(bool paused);
void audio_jingle(unsigned kind);
void audio_effect(unsigned kind);
void audio_sound(unsigned sid,bool loop,float volume);
void audio_stop_loops(void);
void audio_exit(void);
const char *audio_status(void);
#endif
