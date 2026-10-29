#ifndef PORT_AUDIO_H
#define PORT_AUDIO_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Sound is optional: initialization failure leaves the rest of the app usable. */
bool port_audio_init(void);
void port_audio_play_whistle(void);
void port_audio_update(void);
void port_audio_shutdown(void);
const char *port_audio_status(void);
#ifdef __cplusplus
}
#endif
#endif
