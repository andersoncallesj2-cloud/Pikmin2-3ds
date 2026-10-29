// Original Graphics methods extracted from the supplied Pikmin 2 source (CC0).
// Only the SDK/types boundary below is replaced; method bodies are verbatim.
#include <cmath>
#include <cstring>
#include "port_gx.h"
using f32 = float;
struct Vector3f { float x,y,z; Vector3f() = default; explicit Vector3f(float n):x(n),y(n),z(n){} Vector3f(float a,float b,float c):x(a),y(b),z(c){} };
struct Color4 { unsigned char r,g,b,a; };
struct Matrixf {
    struct { PortMtx34 mtxView; } mMatrix;
    void makeSRT(Vector3f scale,Vector3f rotation,Vector3f translation) {
        // The original debug sphere uses unit scale and a single X or Y rotation.
        float sx=std::sin(rotation.x),cx=std::cos(rotation.x);
        float sy=std::sin(rotation.y),cy=std::cos(rotation.y);
        float sz=std::sin(rotation.z),cz=std::cos(rotation.z);
        PortMtx34 m={{cy*cz*scale.x,(sx*sy*cz-cx*sz)*scale.y,(cx*sy*cz+sx*sz)*scale.z,translation.x},
                     {cy*sz*scale.x,(sx*sy*sz+cx*cz)*scale.y,(cx*sy*sz-sx*cz)*scale.z,translation.y},
                     {-sy*scale.x,sx*cy*scale.y,cx*cy*scale.z,translation.z}};
        std::memcpy(mMatrix.mtxView,m,sizeof(m));
    }
};
static void PSMTXConcat(const PortMtx34 a,const PortMtx34 b,PortMtx34 dst) { port_math_concat(dst,a,b); }
#define GXBegin P2_GXBegin
#define GXPosition3f32 P2_GXPosition3f32
#define GXColor4u8 P2_GXColor4u8
#define GXLoadPosMtxImm P2_GXLoadPosMtxImm
#define GX_LINES P2_GX_LINES
#define GX_VTXFMT0 P2_GX_VTXFMT0
struct Graphics {
    Matrixf mMatrix;
    Color4 mDrawColor;
    void drawLine(Vector3f&,Vector3f&);
    void drawSphere(Vector3f&,f32);
};

// BEGIN ORIGINAL METHOD BODIES
void Graphics::drawLine(Vector3f& start, Vector3f& end)
{
	GXBegin(GX_LINES, GX_VTXFMT0, 2);

	GXPosition3f32(start.x, start.y, start.z);
	GXColor4u8(mDrawColor.r, mDrawColor.g, mDrawColor.b, mDrawColor.a);

	GXPosition3f32(end.x, end.y, end.z);
	GXColor4u8(mDrawColor.r, mDrawColor.g, mDrawColor.b, mDrawColor.a);
}

void Graphics::drawSphere(Vector3f& position, f32 radius)
{
	Matrixf concatMtx;
	for (int i = 0; i < 16; i++) {
		Matrixf srtMtx;
		Vector3f scale(1.0f);
		Vector3f rot(0.0f, 0.3926991f * i, 0.0f);

		srtMtx.makeSRT(scale, rot, position);

		PSMTXConcat(mMatrix.mMatrix.mtxView, srtMtx.mMatrix.mtxView, concatMtx.mMatrix.mtxView);
		GXLoadPosMtxImm(concatMtx.mMatrix.mtxView, 0);

		for (int j = 0; j < 16; j++) {
			f32 theta = 0.3926991f * (f32)(j);
			f32 phi   = 0.3926991f * (f32)((j + 1) % 32);
			Vector3f start(radius * sinf(theta), radius * cosf(theta), 0.0f);
			Vector3f end(radius * sinf(phi), radius * cosf(phi), 0.0f);
			drawLine(start, end);
		}
	}

	for (int i = 0; i < 16; i++) {
		Matrixf srtMtx;
		Vector3f scale(1.0f);
		Vector3f rot(0.3926991f * i, 0.0f, 0.0f);

		srtMtx.makeSRT(scale, rot, position);

		PSMTXConcat(mMatrix.mMatrix.mtxView, srtMtx.mMatrix.mtxView, concatMtx.mMatrix.mtxView);
		GXLoadPosMtxImm(concatMtx.mMatrix.mtxView, 0);

		for (int j = 0; j < 16; j++) {
			f32 theta = 0.3926991f * (f32)(j);
			f32 phi   = 0.3926991f * (f32)((j + 1) % 32);
			Vector3f start(radius * sinf(theta), 0.0f, radius * cosf(theta));
			Vector3f end(radius * sinf(phi), 0.0f, radius * cosf(phi));
			drawLine(start, end);
		}
	}
}
// END ORIGINAL METHOD BODIES

extern "C" void port_graphics_sphere(const PortMtx34 view,PortVec3 position,float radius,uint32_t rgba)
{
    Graphics graphics;
    std::memcpy(graphics.mMatrix.mMatrix.mtxView,view,sizeof(PortMtx34));
    graphics.mDrawColor={static_cast<unsigned char>(rgba>>24),static_cast<unsigned char>(rgba>>16),static_cast<unsigned char>(rgba>>8),static_cast<unsigned char>(rgba)};
    Vector3f point(position.x,position.y,position.z);
    graphics.drawSphere(point,radius);
}
