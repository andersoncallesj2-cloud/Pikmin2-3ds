#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_audio.h"
#include "thp_frame.h"

enum { SAMPLE_RATE = 32000, SAMPLE_FRAMES = 5600, AUDIO_CHANNEL = 0 };
static ndspWaveBuf whistle;
static int16_t *pcm;
static bool initialized;
static char status_text[96] = "Audio not initialized";

static void put_be32(uint8_t *p, uint32_t value)
{
    p[0] = value >> 24; p[1] = value >> 16; p[2] = value >> 8; p[3] = value;
}

bool port_audio_init(void)
{
    if (initialized) return true;
    const size_t encoded_size = 80 + (SAMPLE_FRAMES / 14) * 8;
    uint8_t *encoded = calloc(1, encoded_size);
    pcm = linearAlloc(SAMPLE_FRAMES * 2 * sizeof(int16_t));
    if (!encoded || !pcm) {
        free(encoded);
        if (pcm) linearFree(pcm);
        pcm = NULL;
        snprintf(status_text, sizeof(status_text), "Audio disabled: allocation failed");
        return false;
    }
    /* A new, synthetic ADPCM test whistle, not a Nintendo sample. Predictor 0
     * uses zero coefficients; the original game's decoder expands each nibble.
     * The real ported decoder is exercised here before anything reaches NDSP. */
    put_be32(encoded + 4, SAMPLE_FRAMES);
    for (unsigned block = 0; block < SAMPLE_FRAMES / 14; ++block) {
        uint8_t *out = encoded + 80 + block * 8;
        out[0] = 10;  /* predictor 0, 2^10 scale */
        for (unsigned n = 0; n < 14; ++n) {
            unsigned i = block * 14 + n;
            double envelope = fmin(1.0, fmin(i / 200.0, (SAMPLE_FRAMES - 1 - i) / 300.0));
            int q = (int)lround(6.0 * envelope * sin(6.283185307179586 * 880.0 * i / SAMPLE_RATE));
            unsigned nibble = (unsigned)(q & 15);
            out[1 + n / 2] |= n % 2 ? nibble : nibble << 4;
        }
    }
    size_t decoded = port_thp_decode(encoded, encoded_size, pcm, SAMPLE_FRAMES * 2, 0);
    free(encoded);
    if (decoded != SAMPLE_FRAMES) {
        linearFree(pcm); pcm = NULL;
        snprintf(status_text, sizeof(status_text), "Audio disabled: THP decoder failed");
        return false;
    }
    Result result = ndspInit();
    if (R_FAILED(result)) {
        linearFree(pcm); pcm = NULL;
        snprintf(status_text, sizeof(status_text), "Audio off: NDSP %08lX (DSP firmware?)", (unsigned long)result);
        return false;
    }
    initialized = true;
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(AUDIO_CHANNEL);
    ndspChnSetInterp(AUDIO_CHANNEL, NDSP_INTERP_LINEAR);
    ndspChnSetRate(AUDIO_CHANNEL, SAMPLE_RATE);
    ndspChnSetFormat(AUDIO_CHANNEL, NDSP_FORMAT_STEREO_PCM16);
    float mix[12] = {1.0f, 1.0f};
    ndspChnSetMix(AUDIO_CHANNEL, mix);
    DSP_FlushDataCache(pcm, SAMPLE_FRAMES * 2 * sizeof(int16_t));
    memset(&whistle, 0, sizeof(whistle));
    whistle.data_pcm16 = pcm;
    whistle.nsamples = SAMPLE_FRAMES;
    snprintf(status_text, sizeof(status_text), "Audio: THP decoder -> NDSP ready");
    return true;
}

void port_audio_play_whistle(void)
{
    if (!initialized) return;
    if (whistle.status == NDSP_WBUF_QUEUED || whistle.status == NDSP_WBUF_PLAYING) return;
    whistle.status = NDSP_WBUF_FREE;
    ndspChnWaveBufAdd(AUDIO_CHANNEL, &whistle);
}

void port_audio_update(void)
{
    /* NDSP owns playback; the main/game thread never waits for an audio buffer. */
}

void port_audio_shutdown(void)
{
    if (initialized) {
        ndspChnWaveBufClear(AUDIO_CHANNEL);
        ndspExit();
        initialized = false;
    }
    if (pcm) { linearFree(pcm); pcm = NULL; }
    snprintf(status_text, sizeof(status_text), "Audio stopped");
}

const char *port_audio_status(void) { return status_text; }
