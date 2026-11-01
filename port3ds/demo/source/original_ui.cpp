// Original CC0 projectPiki function bodies below; platform shims are NEW.
// Reference commit: 999140f3521e22619547d5d61862a6577ea73882.
// See original_ui_provenance.json and port3ds/LICENSE.source.md.
#include "original_ui.h"
#include <stdint.h>
#include <math.h>
typedef float f32;typedef uint8_t u8;typedef uint32_t u32;
#define HALF_PI 1.5707963267948966f
#define TAU 6.2831853071795865f
#define ROUND_F32_TO_U8(x) ((u8)((x)+.5f))
#define RED_LIFEGAUGE_RATIO .2f
#define YELLOW_LIFEGAUGE_RATIO .5f
#define RED_LIFEGAUGE_COLOR Color4(255,0,0,255)
#define YELLOW_LIFEGAUGE_COLOR Color4(255,255,0,255)
#define GREEN_LIFEGAUGE_COLOR Color4(0,255,0,255)
struct Color4{u8 r,g,b,a;Color4(int R=255,int G=255,int B=255,int A=255):r(R),g(G),b(B),a(A){}};
struct Vector3f{float x,y,z;Vector3f(float X=0,float Y=0,float Z=0):x(X),y(Y),z(Z){}};
static float absF(float v){return fabsf(v);}
struct System{float mDeltaTime;float getDeltaTime(){return mDeltaTime;}};
static System native_system={1.f/30};static System *sys=&native_system;
struct LifeGauge{float mTimer;Color4 mLifeGaugeColor;u8 mCurrentSegmentNum,mMaxSegmentNum;
void init(u8);void update(f32);void draw(f32,f32,f32);void drawOneTri(Vector3f*,Color4&);};
enum{CINFO_Appear,CINFO_Disappear,CINFO_Hidden};enum{CINFOTYPE_Table,CINFOTYPE_Scale};
struct CarryInfoParam{unsigned mUseType;float mYOffsetMax;};struct Graphics{};
struct CarryInfo{float mGrowRate,mYOffset,mScale;u8 mState,mAlpha,mCounter;
void update(const CarryInfoParam&);float drawNumber(Graphics&,f32,f32,int,Color4&,f32);void drawNumberPrim(Graphics&,f32,f32,int,Color4&,f32);};
static OriginalGlyph glyph_callback;static void *glyph_context;static unsigned quad_index;
static struct{float x,y,u,v;Color4 color;} quad[4];static Color4 current_color;
enum{GX_TRIANGLESTRIP,GX_VTXFMT0};
static void GXBegin(int,int,int){quad_index=0;}
static void GXPosition3f32(float x,float y,float){quad[quad_index].x=x;quad[quad_index].y=y;}
static void GXColor4u8(u8 r,u8 g,u8 b,u8 a){current_color=Color4(r,g,b,a);}
static void GXPosition2f32(float u,float v){
    quad[quad_index].u=u;quad[quad_index].v=v;quad[quad_index].color=current_color;
    if(++quad_index==4&&glyph_callback){Color4 c=quad[0].color;
        glyph_callback(glyph_context,quad[0].x,quad[0].y,quad[3].x,quad[3].y,(int)roundf(quad[0].u*12),c.r,c.g,c.b,c.a);}
}
static float *geometry;static unsigned geometry_vertices;
void LifeGauge::drawOneTri(Vector3f *v,Color4&){if(geometry)for(int i=0;i<3;i++){geometry[geometry_vertices*2]=v[i].x;geometry[geometry_vertices*2+1]=v[i].y;geometry_vertices++;}}
void CarryInfo::update(const CarryInfoParam& param)
{
	if (mState != CINFO_Hidden) {
		switch (param.mUseType) {
		case CINFOTYPE_Table: {
			switch (mState) {
			case CINFO_Appear: {
				f32 temp = 0.04f * (param.mYOffsetMax - mYOffset);
				mGrowRate += temp;
				mYOffset += mGrowRate;
				mScale = mYOffset / param.mYOffsetMax;
				break;
			}

			case CINFO_Disappear: {
				f32 temp = 0.04f * -mYOffset;
				mGrowRate += temp;
				mYOffset += mGrowRate;

				if (mYOffset < 0.0f) {
					mGrowRate = 0.0f;
					mYOffset  = 0.0f;
					mScale    = 0.0f;
					mState    = CINFO_Hidden;

				} else {
					mScale = mYOffset / param.mYOffsetMax;
				}
				break;
			}
			}
			f32 modifier = 1.0f;
			mGrowRate *= 0.88f;

			modifier        = (mScale > modifier) ? modifier : mScale;
			f32 scaledAlpha = 255.0f * modifier;
			f32 alpha;
			if (scaledAlpha >= 0.0f) {
				alpha = 0.5f + scaledAlpha;
			} else {
				alpha = scaledAlpha - 0.5f;
			}
			mAlpha = (u8)alpha;
			break;
		}

		case CINFOTYPE_Scale: {
			mCounter++;
			if (mYOffset < param.mYOffsetMax) {
				f32 temp = (param.mYOffsetMax - mYOffset) * 0.04f;
				mGrowRate += temp;
			}
			mYOffset += mGrowRate;
			mGrowRate *= 0.9f;
			mScale = mYOffset / param.mYOffsetMax;
			if (mScale > 1.0f) {
				mScale = 1.0f;
			}

			switch (mState) {
			case CINFO_Appear: {
				f32 scaledAlpha = mScale * 255.0f;
				f32 alpha;
				if (scaledAlpha >= 0.0f) {
					alpha = 0.5f + scaledAlpha;
				} else {
					alpha = scaledAlpha - 0.5f;
				}
				mAlpha = (u8)alpha;
				break;
			}

			case CINFO_Disappear: {
				if (mAlpha > 8) {
					mAlpha -= 8;
				} else {
					mAlpha    = 0;
					mGrowRate = 0.0f;
					mYOffset  = 0.0f;
					mScale    = 0.0f;
					mState    = CINFO_Hidden;
				}
				break;
			}
			}

			break;
		}
		}
	}
}

f32 CarryInfo::drawNumber(Graphics& gfx, f32 x, f32 y, int dispNum, Color4& color, f32 scale)
{
	if (dispNum < 10) {
		// draw single digit
		drawNumberPrim(gfx, x, y, dispNum, color, scale);

		return 7.0f * scale;
	}

	if (dispNum < 100) {
		// draw two digits
		// need to offset digits from center so they're not on top of each other
		f32 xOffset = 5.6f * scale;

		drawNumberPrim(gfx, x - xOffset, y, dispNum / 10, color, scale);
		drawNumberPrim(gfx, x + xOffset, y, dispNum % 10, color, scale);

		return 2.0f * xOffset + 7.0f * scale;
	}

	if (dispNum < 1000) {
		// draw three digits
		// need to offset end digits from center so they're not on top of each other
		f32 xOffset = 12.599999f * scale;

		drawNumberPrim(gfx, x - xOffset, y, dispNum / 100, color, scale);
		drawNumberPrim(gfx, x, y, (dispNum / 10) % 10, color, scale);
		drawNumberPrim(gfx, x + xOffset, y, dispNum % 10, color, scale);

		return 2.0f * xOffset + 7.0f * scale;
	}

	// draw four digits
	// need start and end digits equal from center, and middle two equal from center in between
	f32 xOffsetEnd;
	f32 xOffsetMiddle = 5.6f * scale;
	xOffsetEnd        = 3.0f * xOffsetMiddle;

	drawNumberPrim(gfx, x - xOffsetEnd, y, dispNum / 1000, color, scale);
	drawNumberPrim(gfx, x - xOffsetMiddle, y, (dispNum / 100) % 10, color, scale);
	drawNumberPrim(gfx, x + xOffsetMiddle, y, (dispNum / 10) % 10, color, scale);
	drawNumberPrim(gfx, x + xOffsetEnd, y, dispNum % 10, color, scale);

	return 6.0f * xOffsetMiddle + 7.0f * scale;
}

void CarryInfo::drawNumberPrim(Graphics& gfx, f32 x, f32 y, int digit, Color4& color, f32 scale)
{
	f32 scaledOffset = 7.0f * scale;
	f32 factor       = (1.0f / 12.0f);
	f32 xMin         = x - scaledOffset;
	f32 xMax         = x + scaledOffset;
	f32 yMin         = y - scaledOffset;
	f32 yMax         = y + scaledOffset;
	f32 digitStart   = factor * digit;
	f32 digitEnd     = digitStart + factor;
	f32 zero         = 0.0f;
	f32 one          = 1.0f;

	GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, 4);

	// bottom left
	GXPosition3f32(xMin, yMin, zero);
	GXColor4u8(color.r, color.g, color.b, color.a);
	GXPosition2f32(digitStart, one);

	// bottom right
	GXPosition3f32(xMax, yMin, zero);
	GXColor4u8(color.r, color.g, color.b, color.a);
	GXPosition2f32(digitEnd, one);

	// top left
	GXPosition3f32(xMin, yMax, zero);
	GXColor4u8(color.r, color.g, color.b, color.a);
	GXPosition2f32(digitStart, zero);

	// top right
	GXPosition3f32(xMax, yMax, zero);
	GXColor4u8(color.r, color.g, color.b, color.a);
	GXPosition2f32(digitEnd, zero);
}

void LifeGauge::init(u8 maxSegmentNum)
{
	mTimer             = 0.0f;
	mMaxSegmentNum     = maxSegmentNum;
	mCurrentSegmentNum = maxSegmentNum;
}

void LifeGauge::update(f32 healthRatio)
{
	u32 newSegmentNum = (u8)(f32)(ROUND_F32_TO_U8(mMaxSegmentNum * healthRatio));

	// need to update segment count
	if (mCurrentSegmentNum != (u8)newSegmentNum) {
		f32 delta = absF(((u8)newSegmentNum - mCurrentSegmentNum) / ((f32)mMaxSegmentNum)) * (sys->mDeltaTime * 150.0f);

		// health will update at least every 3 changes, no matter how small said changes are
		if (delta < 0.4f) {
			delta = 0.4f;
		}

		mTimer += delta;

		// only update segment count when we've changed by at least a segment
		if (mTimer > 1.0f) {
			u8 time = mTimer;
			mCurrentSegmentNum += ((u8)newSegmentNum > mCurrentSegmentNum) ? time : -time;
			mTimer -= (u8)time;
		}

		// if we don't have 0 health, always visibly show at least one segment
		if (mCurrentSegmentNum == 0 && healthRatio > 0.0f) {
			mCurrentSegmentNum = 1;
		}
	}

	// update colors
	if (healthRatio < RED_LIFEGAUGE_RATIO) { // lifegauge red
		mLifeGaugeColor = RED_LIFEGAUGE_COLOR;
	} else if (healthRatio < YELLOW_LIFEGAUGE_RATIO) { // lifegauge yellow
		mLifeGaugeColor = YELLOW_LIFEGAUGE_COLOR;
	} else { // lifegauge green
		mLifeGaugeColor = GREEN_LIFEGAUGE_COLOR;
	}
}

void LifeGauge::draw(f32 radius, f32 centerX, f32 centerY)
{
	Vector3f vertices[3];

	// first vertex - we're in 2D drawing mode so z is always 0.0f
	vertices[0] = Vector3f(centerX, centerY, 0.0f);

	// draw segments
	for (int i = 0; i < this->mCurrentSegmentNum; i++) {

		// second vertex is on outside of circle at "start" of segment
		f32 angle     = -HALF_PI - (TAU * ((f32)i / static_cast<f32>(this->mMaxSegmentNum)));
		vertices[1].x = (radius * (f32)cos(angle)) + vertices[0].x;
		vertices[1].y = (radius * (f32)sin(angle)) + vertices[0].y;
		vertices[1].z = vertices[0].z;

		// third vertex is on outside of circle at "end" of segment
		angle         = -HALF_PI - (TAU * ((f32)(i + 1) / static_cast<f32>(this->mMaxSegmentNum)));
		vertices[2].x = (radius * (f32)cos(angle)) + vertices[0].x;
		vertices[2].y = (radius * (f32)sin(angle)) + vertices[0].y;
		vertices[2].z = vertices[0].z;

		// Draw the triangle
		drawOneTri(vertices, mLifeGaugeColor);
	}
}

// Native lifecycle and C bridge. These wrappers are not original game code.
static LifeGauge gauges[7];static CarryInfo carries[32];static float health_clock[7],carry_clock[32];static bool was_visible[32];
extern "C" void original_ui_reset(void){
 for(unsigned i=0;i<7;i++){gauges[i].init(128);gauges[i].update(1);health_clock[i]=0;}
 for(unsigned i=0;i<32;i++){carries[i]=CarryInfo{};carries[i].mState=CINFO_Hidden;was_visible[i]=false;carry_clock[i]=0;}
}
extern "C" void original_health_tick(unsigned i,float ratio,float dt){if(i>=7)return;ratio=fmaxf(0,fminf(1,ratio));health_clock[i]+=dt;while(health_clock[i]>=1.f/30){native_system.mDeltaTime=1.f/30;gauges[i].update(ratio);health_clock[i]-=1.f/30;}}
extern "C" unsigned original_health_segments(unsigned i){return i<7?gauges[i].mCurrentSegmentNum:0;}
extern "C" void original_health_color(unsigned i,u8 out[4]){Color4 c=i<7?gauges[i].mLifeGaugeColor:Color4();out[0]=c.r;out[1]=c.g;out[2]=c.b;out[3]=c.a;}
extern "C" unsigned original_health_geometry(float *out){LifeGauge g;g.init(128);geometry=out;geometry_vertices=0;g.draw(1,0,0);geometry=nullptr;return geometry_vertices;}
extern "C" void original_carry_tick(unsigned i,float dt,bool visible,float height){
 if(i>=32)return;
 CarryInfo &c=carries[i];
 if(visible&&!was_visible[i]){c.mState=CINFO_Appear;c.mCounter=0;}else if(!visible&&was_visible[i])c.mState=CINFO_Disappear;
 was_visible[i]=visible;carry_clock[i]+=dt;CarryInfoParam p={CINFOTYPE_Table,height};
 while(carry_clock[i]>=1.f/30){c.update(p);carry_clock[i]-=1.f/30;}
}
extern "C" void original_carry_pose(unsigned i,float *height,float *scale,float *alpha){if(i>=32){*height=*scale=*alpha=0;return;}*height=carries[i].mYOffset;*scale=carries[i].mScale;*alpha=carries[i].mAlpha/255.f;}
extern "C" void original_carry_draw(unsigned i,unsigned count,unsigned weight,unsigned color,OriginalGlyph emit,void *context){
 if(i>=32||carries[i].mState==CINFO_Hidden)return;
 const u8 colors[7][3]={{30,30,255},{255,0,0},{234,234,0},{255,255,255},{255,255,255},{102,153,153},{102,211,211}};
 if(color>6)color=5;
 Color4 c(colors[color][0],colors[color][1],colors[color][2],carries[i].mAlpha);
 glyph_callback=emit;glyph_context=context;Graphics gfx;CarryInfo &info=carries[i];
 // Original CarryInfo::draw table branch, top-first Pellet layout.
 info.drawNumberPrim(gfx,0,0,10,c,1);
 info.drawNumber(gfx,0,10.150001f,count,c,1.1f);
 info.drawNumber(gfx,0,-7.7000003f,weight,c,.75f);
 glyph_callback=nullptr;
}
