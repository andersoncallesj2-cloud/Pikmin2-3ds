/* Generated from the supplied Pikmin 2 source; see provenance.json. */

#include "math_compat.h"

/* Original: src/Dolphin/mtx/mtx.c :: C_MTXLookAt */
void C_MTXLookAt(Mtx m, const Vec* camPos, const Vec* camUp, const Vec* target)
{

	Vec vLook, vRight, vUp;

	vLook.x = camPos->x - target->x;
	vLook.y = camPos->y - target->y;
	vLook.z = camPos->z - target->z;
	PSVECNormalize(&vLook, &vLook);

	PSVECCrossProduct(camUp, &vLook, &vRight);
	PSVECNormalize(&vRight, &vRight);

	PSVECCrossProduct(&vLook, &vRight, &vUp);

	m[0][0] = vRight.x;
	m[0][1] = vRight.y;
	m[0][2] = vRight.z;
	m[0][3] = -(camPos->x * vRight.x + camPos->y * vRight.y + camPos->z * vRight.z);

	m[1][0] = vUp.x;
	m[1][1] = vUp.y;
	m[1][2] = vUp.z;
	m[1][3] = -(camPos->x * vUp.x + camPos->y * vUp.y + camPos->z * vUp.z);

	m[2][0] = vLook.x;
	m[2][1] = vLook.y;
	m[2][2] = vLook.z;
	m[2][3] = -(camPos->x * vLook.x + camPos->y * vLook.y + camPos->z * vLook.z);
}

/* Original: src/Dolphin/mtx/mtx44.c :: C_MTXPerspective */
void C_MTXPerspective(Mtx44 m, f32 fovY, f32 aspect, f32 n, f32 f)
{
	f32 angle = fovY * 0.5f;
	f32 cot;
	f32 tmp;
	angle   = MTXDegToRad(angle);
	cot     = 1.0f / tanf(angle);
	m[0][0] = cot / aspect;
	m[0][1] = 0.0f;
	m[0][2] = 0.0f;
	m[0][3] = 0.0f;
	m[1][0] = 0.0f;
	m[1][1] = cot;
	m[1][2] = 0.0f;
	m[1][3] = 0.0f;
	m[2][0] = 0.0f;
	m[2][1] = 0.0f;
	tmp     = 1.0f / (f - n);
	m[2][2] = -(n)*tmp;
	m[2][3] = -(f * n) * tmp;
	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = -1.0f;
	m[3][3] = 0.0f;
}

/* Original: src/Dolphin/mtx/mtx44.c :: C_MTXOrtho */
void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f)
{
	f32 tmp = 1.0f / (r - l);
	m[0][0] = 2.0f * tmp;
	m[0][1] = 0.0f;
	m[0][2] = 0.0f;
	m[0][3] = -(r + l) * tmp;
	tmp     = 1.0f / (t - b);
	m[1][0] = 0.0f;
	m[1][1] = 2.0f * tmp;
	m[1][2] = 0.0f;
	m[1][3] = -(t + b) * tmp;
	m[2][0] = 0.0f;
	m[2][1] = 0.0f;
	tmp     = 1.0f / (f - n);
	m[2][2] = -(1.0f) * tmp;
	m[2][3] = -(f)*tmp;
	m[3][0] = 0.0f;
	m[3][1] = 0.0f;
	m[3][2] = 0.0f;
	m[3][3] = 1.0f;
}

/* Original: src/sysCommonU/sysMath.cpp :: qdist2 */
f32 qdist2(f32 x1, f32 y1, f32 x2, f32 y2)
{
	f32 deltaX = x2 - x1;
	f32 deltaY = y2 - y1;

	f32 dist = SQUARE(deltaX) + SQUARE(deltaY);
	if (dist > 0.0f) {
		vf32 calcDist = dist * (__frsqrte(dist));
		dist          = calcDist;
	}

	return dist;
}

/* Original: src/sysCommonU/sysMath.cpp :: roundAng */
f32 roundAng(f32 angle)
{
	// if < 0, add 2PI
	if (angle < 0.0f) {
		angle += TAU;
	}

	// if > 2PI, subtract it
	if (angle >= TAU) {
		angle -= TAU;
	}

	return angle;
}

/* Original: src/sysCommonU/sysMath.cpp :: angDist */
f32 angDist(f32 angle1, f32 angle2)
{
	f32 angle = roundAng(angle1 - angle2);

	if (angle >= PI) {
		angle = -roundAng(TAU - angle);
	}
	return angle;
}
