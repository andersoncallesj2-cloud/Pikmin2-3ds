#ifndef PORT_THP_AUDIO_COMPAT_H
#define PORT_THP_AUDIO_COMPAT_H
#include <stdint.h>
#include <stddef.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef struct THPAudioRecordHeader {
    u32 offsetNextChannel;
    u32 sampleSize;
    s16 lCoef[8][2];
    s16 rCoef[8][2];
    s16 lYn1, lYn2, rYn1, rYn2;
} THPAudioRecordHeader;
typedef struct THPAudioDecodeInfo {
    u8 *encodeData;
    u32 offsetNibbles;
    u8 predictor, scale;
    s16 yn1, yn2;
} THPAudioDecodeInfo;
u32 THPAudioDecode(s16 *, u8 *, s32);
#ifdef PORT_THP_DECODER_INTERNAL
static s32 __THPAudioGetNewSample(THPAudioDecodeInfo *);
static void __THPAudioInitialize(THPAudioDecodeInfo *, u8 *);
#endif
#endif
