#ifndef PORT_GX_H
#define PORT_GX_H
#include <stdbool.h>
#include <stdint.h>
#include "port_math.h"
#ifdef __cplusplus
extern "C" {
#endif
/* First implemented GX direct-vertex path: untextured position + RGBA lines.
 * Unsupported GX state is not silently accepted by this narrow interface. */
enum { P2_GX_LINES = 0xa8, P2_GX_VTXFMT0 = 0 };
void P2_GXBegin(int primitive, int format, unsigned count);
void P2_GXPosition3f32(float x, float y, float z);
void P2_GXColor4u8(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void P2_GXLoadPosMtxImm(const PortMtx34 matrix, unsigned index);
bool port_gfx_init(void);
void port_gfx_shutdown(void);
void port_gfx_begin(const PortMtx34 view);
void port_gfx_end(void);
unsigned port_gfx_errors(void);
void port_gfx_triangle(PortVec3 a, PortVec3 b, PortVec3 c, uint32_t rgba);
void port_gfx_box(PortVec3 center, float x, float y, float z, uint32_t rgba);
void port_graphics_sphere(const PortMtx34 view, PortVec3 position, float radius, uint32_t rgba);
#ifdef __cplusplus
}
#endif
#endif
