#ifndef PORT_THP_FRAME_H
#define PORT_THP_FRAME_H
#include <stdint.h>
#include <stddef.h>
/* Checked wrapper around the game's THPAudioDecode. Input is a big-endian
 * THP audio component, not an entire .thp movie. Output preserves the original
 * routine's right,left channel order. Returns samples per channel, or 0. */
size_t port_thp_decode(const uint8_t *frame, size_t frame_size,
                       int16_t *output, size_t output_samples, int planar);
#endif
