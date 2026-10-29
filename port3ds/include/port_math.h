#ifndef PIKMIN2_PORT_MATH_H
#define PIKMIN2_PORT_MATH_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PortVec3 { float x, y, z; } PortVec3;
typedef float PortMtx34[3][4];
typedef float PortMtx44[4][4];

/* Original Pikmin 2 game routines, with the original one-turn range contract.
 * roundAng's input must be in [-2*pi, 4*pi); angDist's arguments in [0, 2*pi).
 * Use the checked convenience wrappers below with unconstrained inputs. */
float qdist2(float x1, float z1, float x2, float z2);
float roundAng(float angle);
float angDist(float angle1, float angle2);

/* Original Dolphin C camera/projection routines, linked from source extracts.
 * Projection matrices use the GameCube [-1,0] depth convention; a 3DS renderer
 * must explicitly adapt depth/viewport conventions before passing these to GPU. */
void C_MTXLookAt(PortMtx34 m, const PortVec3* eye, const PortVec3* up, const PortVec3* target);
void C_MTXPerspective(PortMtx44 m, float fov_degrees, float aspect, float near_z, float far_z);
void C_MTXOrtho(PortMtx44 m, float top, float bottom, float left, float right, float near_z, float far_z);

/* Validate the camera before calling the original code. Output stays unchanged
 * on invalid/degenerate input; the caller may keep its previous valid camera. */
bool port_math_camera(PortMtx34 view, const PortVec3* eye, const PortVec3* up, const PortVec3* target);
float port_math_distance_xz(const PortVec3* a, const PortVec3* b);
float port_math_turn_toward(float current, float target, float max_step);
/* Scalar implementation of original PowerPC PSMTXConcat semantics; permits
 * out==a or out==b. This is a portability adapter, not an original C body. */
void port_math_concat(PortMtx34 out, const PortMtx34 a, const PortMtx34 b);

#ifdef __cplusplus
}
#endif
#endif
