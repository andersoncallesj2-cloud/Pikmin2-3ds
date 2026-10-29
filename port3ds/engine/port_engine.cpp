#include "engine_compat.h"
#include <cmath>
#include <cstring>

namespace P2Engine {
struct Context {
    PortEngineCallbacks callbacks;
    PortEngineStats stats;
    uint32_t frame_limit;
    PortEnginePhase phase;
    Section* section;
    bool stopped;
    bool frame_open;
    bool rendering_open;
    AudioSceneManager audio;

    void requestExit(bool limit)
    {
        stopped = true;
        stats.exit_requested = true;
        stats.frame_limit_hit = stats.frame_limit_hit || limit;
        section->mIsMainActive = false;
    }
};

System* sys = nullptr;
bool gTHPReaderDvdAccess = false;
static Context* active_context;

void Timers::newFrame()
{
    /* Performance instrumentation is not implemented by this initial backend. */
}

void Fader::startFadeIn(s32 frames)
{
    if (!context->stopped && context->callbacks.fade)
        context->callbacks.fade(context->callbacks.context, true, (unsigned)frames);
}

void Fader::startFadeOut(s32 frames)
{
    if (!context->stopped && context->callbacks.fade)
        context->callbacks.fade(context->callbacks.context, false, (unsigned)frames);
}

void AudioSceneManager::doStopMainSeqCheck(s32 frames)
{
    if (context->callbacks.stop_audio)
        context->callbacks.stop_audio(context->callbacks.context, (unsigned)frames);
}

AudioSceneManager* PSMGetSceneMgrCheck()
{
    return &active_context->audio;
}

bool System::beginFrame()
{
    if (context->stopped) return false;
    if (context->frame_limit && context->stats.frames >= context->frame_limit) {
        context->requestExit(true);
        return false;
    }
    ++context->stats.frames;
    if (context->phase == PORT_ENGINE_MAIN) ++context->stats.main_frames;
    context->frame_open = true;
    if (context->callbacks.begin_frame && !context->callbacks.begin_frame(context->callbacks.context)) {
        context->requestExit(false);
        return false;
    }
    return true;
}

void System::beginRender()
{
    if (!context->frame_open || context->stopped) return;
    context->rendering_open = true;
    if (context->callbacks.begin_render)
        context->callbacks.begin_render(context->callbacks.context);
}

void System::endRender()
{
    if (!context->rendering_open) return;
    if (context->callbacks.end_render)
        context->callbacks.end_render(context->callbacks.context);
    context->rendering_open = false;
}

void System::endFrame()
{
    if (!context->frame_open) return;
    if (context->callbacks.end_frame)
        context->callbacks.end_frame(context->callbacks.context);
    context->frame_open = false;
}

/* A platform section substitutes only concrete content/service endpoints. The
 * scheduling code itself is the uploaded retail Section implementation. */
class PlatformSection final : public Section {
public:
    explicit PlatformSection(Context& c, Display& display, Graphics& graphics, float fade_seconds) : context(c)
    {
        mDisplayWiper = nullptr;
        mDisplay = &display;
        mGraphics = &graphics;
        mTimeStep = fade_seconds;
        mIsMainActive = true;
        mIsLoadingDVD = false;
    }

    void drawInit(Graphics&, EDrawInitMode mode) override
    {
        context.phase = static_cast<PortEnginePhase>((int)mode + 1);
        if (!context.stopped && context.callbacks.draw_init)
            context.callbacks.draw_init(context.callbacks.context, context.phase);
    }

    void doLoadingStart() override
    {
        context.phase = PORT_ENGINE_LOADING;
        if (context.callbacks.loading_start)
            context.callbacks.loading_start(context.callbacks.context);
    }

    bool doLoading() override
    {
        return !context.stopped && context.callbacks.loading_step
            && context.callbacks.loading_step(context.callbacks.context);
    }

    bool doUpdate() override
    {
        if (context.stopped) return false;
        ++context.stats.update_calls;
        bool keep_running = context.callbacks.update
            && context.callbacks.update(context.callbacks.context);
        if (!keep_running) mIsMainActive = false;
        return keep_running;
    }

    void doDraw(Graphics&) override
    {
        if (context.stopped) return;
        ++context.stats.draw_calls;
        if (context.callbacks.draw)
            context.callbacks.draw(context.callbacks.context, context.phase);
    }

    bool isFinishable() override
    {
        return context.stopped || !context.callbacks.is_finishable
            || context.callbacks.is_finishable(context.callbacks.context);
    }

private:
    Context& context;
};
} // namespace P2Engine

extern "C" bool port_engine_run(const PortEngineCallbacks* callbacks, float fade_seconds,
                                uint32_t max_frames, PortEngineStats* out_stats)
{
    using namespace P2Engine;
    if (out_stats) std::memset(out_stats, 0, sizeof(*out_stats));
    if (!callbacks || !std::isfinite(fade_seconds) || fade_seconds < 0 || fade_seconds > 10 || active_context)
        return false;
    Context context = {};
    context.callbacks = *callbacks;
    context.frame_limit = max_frames;
    context.audio.context = &context;
    Timers timers = {&context};
    CardManager cards;
    System system = {&context, &timers, &cards};
    Fader fader = {&context};
    Display display = {callbacks->fade ? &fader : nullptr};
    Graphics graphics;
    PlatformSection section(context, display, graphics, fade_seconds);
    context.section = &section;
    active_context = &context;
    sys = &system;
    section.init();
    section.run();
    sys = nullptr;
    active_context = nullptr;
    if (out_stats) *out_stats = context.stats;
    return true;
}
