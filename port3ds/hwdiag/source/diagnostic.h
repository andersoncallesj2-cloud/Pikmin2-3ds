#pragma once
#include <3ds.h>
#include <stddef.h>
typedef enum { NOT_RUN, CHECK_PASS, CHECK_FAIL } Check;
typedef struct {
    Check gpu, target, shader, program, uniform, vbo, attributes, buffer, flush, frame, draw_on;
    uintptr_t physical;
    size_t stride, bytes;
    unsigned vertices, submitted, failures, busy_ms;
    Result last_result;
    const char *last_error;
    int buffer_id;
    float cpu_ms, gpu_ms, command_usage;
} DiagnosticStatus;
const DiagnosticStatus *diag_status(void);
void diag_options(unsigned test, bool interleaved, bool depth);
bool diag_ready(void);
