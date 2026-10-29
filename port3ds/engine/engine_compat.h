#ifndef PIKMIN2_ENGINE_COMPAT_H
#define PIKMIN2_ENGINE_COMPAT_H

#include "port_engine.h"
#include <cassert>
#include <cstdint>

#define SINGLE_FRAME_LENGTH (1.0f / 60.0f)
#define ROUND_F32_TO_U8(a) (((a) >= 0.0f) ? ((a) + 0.5f) : ((a) - 0.5f))
#define JUT_ASSERTLINE(line, condition, message) assert(condition)

namespace P2Engine {
typedef float f32;
typedef int32_t s32;
struct Graphics {};
struct Context;

struct Timers {
    Context* context;
    void newFrame();
    void _start(const char*, bool) {}
    void _stop(const char*) {}
};
struct CardManager { void update() {} };
struct Wiper { void update() {} };
struct Fader {
    Context* context;
    void startFadeIn(s32 frames);
    void startFadeOut(s32 frames);
};
struct Display {
    Fader* mFader;
    /* The platform's begin/end-frame callbacks own actual VBlank handling. */
    void waitBlanking(int) {}
};
struct AudioSceneManager {
    Context* context;
    void doStopMainSeqCheck(s32 frames);
};

struct System {
    Context* context;
    Timers* mTimers;
    CardManager* mCardMgr;
    bool beginFrame();
    void endFrame();
    void beginRender();
    void endRender();
    /* No DVD/memory-card device is mounted in this initial platform backend. */
    bool isDvdErrorOccured() { return false; }
    bool dvdLoadSyncAllNoBlock() { return false; }
};
extern System* sys;
extern bool gTHPReaderDvdAccess;
AudioSceneManager* PSMGetSceneMgrCheck();

/* Narrow source compatibility declaration. Its constructor/destructor and
 * physical heap/display layout are platform adapters; original retail lifecycle
 * method bodies are compiled verbatim in original_section.cpp. */
struct Section {
    enum EDrawInitMode { Zero = 0, One = 1, Two = 2 };
    virtual ~Section() {}
    virtual void drawInit(Graphics&, EDrawInitMode) = 0;
    virtual void doLoadingStart() = 0;
    virtual bool doLoading() = 0;
    virtual bool doUpdate() = 0;
    virtual void doDraw(Graphics&) = 0;
    virtual bool isFinishable() = 0;
    void init();
    void run();
    void loading();
    void fadeIn();
    void main();
    void fadeOut();
    bool beginFrame();
    void endFrame();
    void beginRender();
    void endRender();
    bool update();
    void draw(Graphics&);
    Wiper* mDisplayWiper;
    Display* mDisplay;
    Graphics* mGraphics;
    f32 mTimeStep;
    volatile bool mIsMainActive;
    bool mIsLoadingDVD;
};

} // namespace P2Engine
#endif
