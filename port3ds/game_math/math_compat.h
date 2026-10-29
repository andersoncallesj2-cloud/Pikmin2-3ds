#ifndef PIKMIN2_MATH_COMPAT_H
#define PIKMIN2_MATH_COMPAT_H

#include "port_math.h"
#include <math.h>

typedef float f32;
typedef volatile float vf32;
typedef PortVec3 Vec;
typedef PortMtx34 Mtx;
typedef PortMtx44 Mtx44;

#define SQUARE(x) ((x) * (x))
/* Values copied from include/trig.h and include/Dolphin/mtx.h. */
#define PI 3.1415927f
#define TAU 6.2831855f
#define MTXDegToRad(a) ((a) * 0.01745329252f)

/* Scalar ARM replacements for the GameCube paired-single/estimate operations.
 * These are explicit portability implementations, not original assembly. */
static inline float __frsqrte(float value) { return 1.0f / sqrtf(value); }
void PSVECNormalize(const Vec* in, Vec* out);
void PSVECCrossProduct(const Vec* a, const Vec* b, Vec* out);

#endif
