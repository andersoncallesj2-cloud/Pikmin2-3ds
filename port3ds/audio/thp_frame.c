#include "thp_frame.h"
#include "compat/THP/THPAudio.h"
#include <stdlib.h>
#include <string.h>

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

size_t port_thp_decode(const uint8_t *frame, size_t size,
                       int16_t *output, size_t capacity, int planar)
{
    enum { HEADER = 80, MAX_FRAMES = 32768 };
    if (!frame || !output || size < HEADER) return 0;
    uint32_t stride = be32(frame), samples = be32(frame + 4);
    if (!samples || samples > MAX_FRAMES || capacity / 2 < samples) return 0;
    size_t bytes = ((samples + 13) / 14) * 8;
    size_t payload = size - HEADER;
    if (payload < bytes) return 0;
    if (stride && (stride < bytes || stride > payload || payload - stride < bytes)) return 0;
    size_t used = stride ? (size_t)stride + bytes : bytes;
    if (used > MAX_FRAMES * 2u) return 0;
    THPAudioRecordHeader header;
    _Static_assert(sizeof(header) == HEADER, "THP header layout must match source");
    header.offsetNextChannel = stride;
    header.sampleSize = samples;
    for (int predictor = 0; predictor < 8; ++predictor)
        for (int tap = 0; tap < 2; ++tap) {
            header.lCoef[predictor][tap] = (int16_t)be16(frame + 8 + predictor * 4 + tap * 2);
            header.rCoef[predictor][tap] = (int16_t)be16(frame + 40 + predictor * 4 + tap * 2);
        }
    header.lYn1 = (int16_t)be16(frame + 72);
    header.lYn2 = (int16_t)be16(frame + 74);
    header.rYn1 = (int16_t)be16(frame + 76);
    header.rYn2 = (int16_t)be16(frame + 78);
    uint8_t *native = malloc(HEADER + used);
    if (!native) return 0;
    memcpy(native, &header, HEADER);
    memcpy(native + HEADER, frame + HEADER, used);
    size_t result = THPAudioDecode(output, native, planar ? 1 : 0);
    free(native);
    return result;
}
