#ifndef PIKMIN2_PORT_ENGINE_H
#define PIKMIN2_PORT_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PortEnginePhase {
    PORT_ENGINE_LOADING = 0,
    PORT_ENGINE_FADE_IN = 1,
    PORT_ENGINE_MAIN = 2,
    PORT_ENGINE_FADE_OUT = 3
} PortEnginePhase;

typedef struct PortEngineCallbacks {
    void* context;
    bool (*begin_frame)(void* context);
    void (*begin_render)(void* context);
    void (*draw)(void* context, PortEnginePhase phase);
    bool (*update)(void* context); /* False ends the original main loop. */
    void (*end_render)(void* context);
    void (*end_frame)(void* context);
    void (*draw_init)(void* context, PortEnginePhase phase);
    void (*loading_start)(void* context);
    bool (*loading_step)(void* context); /* True means more loading frames. */
    void (*fade)(void* context, bool fade_in, unsigned frames);
    void (*stop_audio)(void* context, unsigned frames);
    bool (*is_finishable)(void* context);
} PortEngineCallbacks;

typedef struct PortEngineStats {
    uint32_t frames;
    uint32_t main_frames;
    uint32_t draw_calls;
    uint32_t update_calls;
    bool frame_limit_hit;
    bool exit_requested;
} PortEngineStats;

/* Execute the uploaded retail Section::run/loading/fadeIn/main/fadeOut loop.
 * fade_seconds must be finite and in [0,10]. Set 0 for an unfaded bring-up.
 * max_frames==0 means no artificial limit; a positive budget includes loading.
 * begin_frame false requests exit. Remaining original unwind phases execute,
 * but the adapter suppresses draw/update callbacks after that request. The
 * begin/end-render and end-frame callbacks still pair for an opened frame.
 * Call from one thread only; nested/reentrant runs are rejected. */
bool port_engine_run(const PortEngineCallbacks* callbacks, float fade_seconds,
                     uint32_t max_frames, PortEngineStats* out_stats);

#ifdef __cplusplus
}
#endif
#endif
