#include "port_engine.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

struct Probe {
    std::string events;
    int begin_count = 0;
    int update_count = 0;
    int loads = 0;
    int stop_after_updates = 2;
    int exit_on_frame = 0;
    bool load_forever = false;
    bool finishable = true;
    bool nested_rejected = false;
    bool test_nested = false;
};

static Probe& probe(void* ctx) { return *static_cast<Probe*>(ctx); }
static bool begin(void* ctx)
{
    Probe& p = probe(ctx);
    p.events += 'B';
    ++p.begin_count;
    if (p.test_nested) {
        PortEngineCallbacks nested = {};
        PortEngineStats stats;
        p.nested_rejected = !port_engine_run(&nested, 0, 1, &stats);
    }
    return !p.exit_on_frame || p.begin_count < p.exit_on_frame;
}
static void begin_render(void* ctx) { probe(ctx).events += 'R'; }
static void draw(void* ctx, PortEnginePhase phase)
{
    probe(ctx).events += phase == PORT_ENGINE_MAIN ? 'D' : phase == PORT_ENGINE_FADE_IN ? 'I' : 'O';
}
static bool update(void* ctx)
{
    Probe& p = probe(ctx);
    p.events += 'U';
    return ++p.update_count < p.stop_after_updates;
}
static void end_render(void* ctx) { probe(ctx).events += 'E'; }
static void end_frame(void* ctx) { probe(ctx).events += 'F'; }
static void draw_init(void* ctx, PortEnginePhase phase)
{
    probe(ctx).events += phase == PORT_ENGINE_MAIN ? 'm' : phase == PORT_ENGINE_FADE_IN ? 'i' : 'o';
}
static void loading_start(void* ctx) { probe(ctx).events += 'L'; }
static bool loading_step(void* ctx)
{
    Probe& p = probe(ctx);
    p.events += 'l';
    ++p.loads;
    return p.load_forever;
}
static void fade(void* ctx, bool fade_in, unsigned frames)
{
    assert(frames == 0 || frames == 1);
    probe(ctx).events += fade_in ? '+' : '-';
}
static void stop_audio(void* ctx, unsigned frames)
{
    assert(frames == 0 || frames == 1);
    probe(ctx).events += 'S';
}
static bool finishable(void* ctx) { return probe(ctx).finishable; }

static PortEngineCallbacks callbacks(Probe& p)
{
    PortEngineCallbacks c = {};
    c.context = &p;
    c.begin_frame = begin;
    c.begin_render = begin_render;
    c.draw = draw;
    c.update = update;
    c.end_render = end_render;
    c.end_frame = end_frame;
    c.draw_init = draw_init;
    c.loading_start = loading_start;
    c.loading_step = loading_step;
    c.fade = fade;
    c.stop_audio = stop_audio;
    c.is_finishable = finishable;
    return c;
}

int main()
{
    {
        Probe p;
        PortEngineCallbacks c = callbacks(p);
        PortEngineStats s;
        assert(port_engine_run(&c, 0, 20, &s));
        /* Original retail loading -> fadeIn -> main -> fadeOut scheduling.
         * In every main frame, drawing and endRender precede update. */
        assert(p.events == "LBRElFi+mBRDEUFBRDEUFS-o");
        assert(s.frames == 3 && s.main_frames == 2 && s.draw_calls == 2 && s.update_calls == 2);
        assert(!s.exit_requested && !s.frame_limit_hit);
    }
    {
        Probe p;
        PortEngineCallbacks c = callbacks(p);
        PortEngineStats s;
        assert(port_engine_run(&c, 1.0f/60.0f, 20, &s));
        assert(p.events == "LBRElFi+BRIEFmBRDEUFBRDEUFS-oBROEUF");
        assert(s.frames == 5 && s.main_frames == 2 && s.draw_calls == 4 && s.update_calls == 3);
    }
    {
        Probe p;
        p.stop_after_updates = 1000;
        PortEngineCallbacks c = callbacks(p);
        PortEngineStats s;
        assert(port_engine_run(&c, 0, 2, &s));
        assert(s.frames == 2 && s.main_frames == 1 && s.update_calls == 1);
        assert(s.exit_requested && s.frame_limit_hit);
    }
    {
        Probe p;
        p.load_forever = true;
        p.finishable = false;
        PortEngineCallbacks c = callbacks(p);
        PortEngineStats s;
        assert(port_engine_run(&c, 0, 3, &s));
        assert(s.frames == 3 && s.main_frames == 0 && s.update_calls == 0);
        assert(s.frame_limit_hit && s.exit_requested);
    }
    {
        Probe p;
        p.exit_on_frame = 2;
        PortEngineCallbacks c = callbacks(p);
        PortEngineStats s;
        assert(port_engine_run(&c, 0, 0, &s));
        assert(s.exit_requested && !s.frame_limit_hit && s.frames == 2);
        assert(s.draw_calls == 0 && s.update_calls == 0);
        assert(p.events == "LBRElFi+mBFS");
    }
    {
        Probe p;
        p.test_nested = true;
        PortEngineCallbacks c = callbacks(p);
        assert(port_engine_run(&c, 0, 10, nullptr));
        assert(p.nested_rejected);
        assert(!port_engine_run(nullptr, 0, 0, nullptr));
        assert(!port_engine_run(&c, NAN, 0, nullptr));
        assert(!port_engine_run(&c, -1, 0, nullptr));
        assert(!port_engine_run(&c, 11, 0, nullptr));
    }
    std::puts("Original Section lifecycle tests passed (6 scenarios: ordering, fades, frame budget, loading timeout, exit, reentrancy).");
}
