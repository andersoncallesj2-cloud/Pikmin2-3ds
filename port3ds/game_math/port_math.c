#include "math_compat.h"
#include <float.h>
#include <string.h>

void PSVECNormalize(const Vec* in, Vec* out)
{
    float scale = fmaxf(fabsf(in->x), fmaxf(fabsf(in->y), fabsf(in->z)));
    if (scale == 0.0f || !isfinite(scale)) {
        *out = (Vec){0.0f, 0.0f, 0.0f};
        return;
    }
    /* Scaling avoids overflow for finite large inputs and underflow for small ones. */
    float x = in->x / scale, y = in->y / scale, z = in->z / scale;
    float inverse = 1.0f / sqrtf(x*x + y*y + z*z);
    *out = (Vec){x * inverse, y * inverse, z * inverse};
}

void PSVECCrossProduct(const Vec* a, const Vec* b, Vec* out)
{
    Vec value = {a->y * b->z - a->z * b->y,
                 a->z * b->x - a->x * b->z,
                 a->x * b->y - a->y * b->x};
    *out = value;
}

static bool finite_vec(const Vec* v)
{
    return v && isfinite(v->x) && isfinite(v->y) && isfinite(v->z);
}

bool port_math_camera(PortMtx34 view, const PortVec3* eye, const PortVec3* up, const PortVec3* target)
{
    if (!view || !finite_vec(eye) || !finite_vec(up) || !finite_vec(target)) return false;
    Vec direction = {eye->x - target->x, eye->y - target->y, eye->z - target->z};
    if (!finite_vec(&direction)) return false;
    Vec look, up_normal, cross;
    PSVECNormalize(&direction, &look);
    PSVECNormalize(up, &up_normal);
    PSVECCrossProduct(&up_normal, &look, &cross);
    float cross_length2 = cross.x*cross.x + cross.y*cross.y + cross.z*cross.z;
    if (cross_length2 < 1.0e-10f) return false;
    PortMtx34 candidate;
    C_MTXLookAt(candidate, eye, &up_normal, target);
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col)
            if (!isfinite(candidate[row][col])) return false;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col)
            view[row][col] = candidate[row][col];
    return true;
}

float port_math_distance_xz(const PortVec3* a, const PortVec3* b)
{
    if (!finite_vec(a) || !finite_vec(b)) return INFINITY;
    double x = (double)b->x - a->x, z = (double)b->z - a->z;
    if (x*x + z*z > FLT_MAX) return INFINITY;
    return qdist2(a->x, a->z, b->x, b->z);
}

static float normal_angle(float angle)
{
    float wrapped = fmodf(angle, TAU);
    return roundAng(wrapped);
}

float port_math_turn_toward(float current, float target, float max_step)
{
    if (!isfinite(current) || !isfinite(target) || !isfinite(max_step) || max_step < 0.0f) return current;
    current = normal_angle(current);
    target = normal_angle(target);
    float difference = angDist(target, current);
    if (fabsf(difference) <= max_step) return target;
    return normal_angle(current + copysignf(max_step, difference));
}

void port_math_concat(PortMtx34 out, const PortMtx34 a, const PortMtx34 b)
{
    PortMtx34 result;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col)
            result[row][col] = a[row][0]*b[0][col] + a[row][1]*b[1][col] + a[row][2]*b[2][col];
        result[row][3] = a[row][0]*b[0][3] + a[row][1]*b[1][3] + a[row][2]*b[2][3] + a[row][3];
    }
    memcpy(out, result, sizeof(result));
}
