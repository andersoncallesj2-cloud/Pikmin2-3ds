#include "thp_frame.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "audio test failed at line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static void put16(uint8_t *p, int value) { p[0] = (uint16_t)value >> 8; p[1] = value; }
static void put32(uint8_t *p, uint32_t value)
{
    p[0] = value >> 24; p[1] = value >> 16; p[2] = value >> 8; p[3] = value;
}

int main(void)
{
    uint8_t frame[112] = {0};
    int16_t output[64];
    put32(frame + 4, 14);
    frame[80] = 2;
    for (int i = 0; i < 7; ++i) frame[81 + i] = ((i * 2) << 4) | (i * 2 + 1);
    CHECK(port_thp_decode(frame, 88, output, 28, 0) == 14);
    for (int i = 0; i < 14; ++i) {
        int value = (i < 8 ? i : i - 16) * 4;
        CHECK(output[i * 2] == value);
        CHECK(output[i * 2 + 1] == value);
    }
    /* A second ADPCM block must consume a fresh predictor/scale header. */
    put32(frame + 4, 28);
    frame[80] = 0;
    memset(frame + 81, 0x11, 7);
    frame[88] = 1;
    memset(frame + 89, 0xff, 7);
    CHECK(port_thp_decode(frame, 96, output, 56, 0) == 28);
    for (int i = 0; i < 28; ++i) CHECK(output[i * 2] == (i < 14 ? 1 : -2));

    /* Stereo preserves the source's right,left order; test planar mode too. */
    memset(frame, 0, sizeof(frame));
    put32(frame, 8); put32(frame + 4, 14);
    frame[80] = 2; memset(frame + 81, 0x11, 7);
    frame[88] = 1; memset(frame + 89, 0xff, 7);
    CHECK(port_thp_decode(frame, 96, output, 28, 0) == 14);
    for (int i = 0; i < 14; ++i) { CHECK(output[i * 2] == -2); CHECK(output[i * 2 + 1] == 4); }
    CHECK(port_thp_decode(frame, 96, output, 28, 1) == 14);
    for (int i = 0; i < 14; ++i) { CHECK(output[i] == -2); CHECK(output[14 + i] == 4); }

    /* Predictive coefficient, initial history and BE conversion. */
    memset(frame, 0, sizeof(frame)); put32(frame + 4, 14);
    put16(frame + 8, 2048); put16(frame + 72, 1000);
    CHECK(port_thp_decode(frame, 88, output, 28, 0) == 14);
    for (int i = 0; i < 28; ++i) CHECK(output[i] == 1000);

    /* Full-scale signed saturation, including the largest supported exponent. */
    memset(frame, 0, sizeof(frame)); put32(frame + 4, 14);
    frame[80] = 15; memset(frame + 81, 0x78, 7);
    CHECK(port_thp_decode(frame, 88, output, 28, 0) == 14);
    for (int i = 0; i < 14; ++i) CHECK(output[i * 2] == (i % 2 ? -32768 : 32767));

    CHECK(port_thp_decode(NULL, 0, output, 28, 0) == 0);
    CHECK(port_thp_decode(frame, 88, NULL, 28, 0) == 0);
    CHECK(port_thp_decode(frame, 79, output, 28, 0) == 0);
    CHECK(port_thp_decode(frame, 87, output, 28, 0) == 0);
    CHECK(port_thp_decode(frame, 88, output, 27, 0) == 0);
    put32(frame + 4, 0xffffffff);
    CHECK(port_thp_decode(frame, 88, output, 28, 0) == 0);
    put32(frame + 4, 14); put32(frame, 4);
    CHECK(port_thp_decode(frame, 96, output, 28, 0) == 0);
    put32(frame, 0xffffffff);
    CHECK(port_thp_decode(frame, 96, output, 28, 0) == 0);
    put32(frame, 8);
    CHECK(port_thp_decode(frame, 95, output, 28, 0) == 0);
    printf("Audio decoder: %u independent vector/bounds checks passed.\n", checks);
    return 0;
}
