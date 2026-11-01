#ifndef DEMO_AUDIO_H
#define DEMO_AUDIO_H
#include <stdbool.h>
bool audio_init(void);
void audio_update(bool paused);
void audio_jingle(unsigned kind);
void audio_effect(unsigned kind);
void audio_exit(void);
const char *audio_status(void);
#endif
